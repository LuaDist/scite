// SciTE - Scintilla based Text Editor
/** @file FileWorker.cxx
 ** Implementation of classes to perform background file tasks as threads.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#if defined(__unix__)

#include <unistd.h>

#else

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
#endif

#endif

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"
#include "SString.h"

#include "FilePath.h"
#include "Cookie.h"
#include "Worker.h"
#include "FileWorker.h"
#include "Utf8_16.h"

const double timeBetweenProgress = 0.4;

FileWorker::FileWorker(WorkerListener *pListener_, FilePath path_, long size_, FILE *fp_) :
	pListener(pListener_), path(path_), size(size_), err(0), fp(fp_), sleepTime(0), nextProgress(timeBetweenProgress) {
}

FileWorker::~FileWorker() {
}

double FileWorker::Duration() {
	return et.Duration();
}

FileLoader::FileLoader(WorkerListener *pListener_, ILoader *pLoader_, FilePath path_, long size_, FILE *fp_) : 
	FileWorker(pListener_, path_, size_, fp_), pLoader(pLoader_), readSoFar(0), unicodeMode(uni8Bit) {
	jobSize = static_cast<int>(size);
	jobProgress = 0;
}

FileLoader::~FileLoader() {
}

void FileLoader::Execute() {
	if (fp) {
		Utf8_16_Read convert;
		std::vector<char> data(blockSize);
		size_t lenFile = fread(&data[0], 1, blockSize, fp);
		UniMode umCodingCookie = CodingCookieValue(&data[0], lenFile);
		while ((lenFile > 0) && (err == 0) && (!cancelling)) {
#ifdef __unix__
			usleep(sleepTime * 1000);
#else
			::Sleep(sleepTime);
#endif
			lenFile = convert.convert(&data[0], lenFile);
			char *dataBlock = convert.getNewBuf();
			err = pLoader->AddData(dataBlock, static_cast<int>(lenFile));
			jobProgress += static_cast<int>(lenFile);
			if (et.Duration() > nextProgress) {
				nextProgress = et.Duration() + timeBetweenProgress;
				pListener->PostOnMainThread(WORK_FILEPROGRESS, this);
			}
			lenFile = fread(&data[0], 1, blockSize, fp);
		}
		fclose(fp);
		fp = 0;
		unicodeMode = static_cast<UniMode>(
		            static_cast<int>(convert.getEncoding()));
		// Check the first two lines for coding cookies
		if (unicodeMode == uni8Bit) {
			unicodeMode = umCodingCookie;
		}
	}
	completed = true;
	pListener->PostOnMainThread(WORK_FILEREAD, this);
}

void FileLoader::Cancel() {
	FileWorker::Cancel();
	pLoader->Release();
	pLoader = 0;
}

FileStorer::FileStorer(WorkerListener *pListener_, const char *documentBytes_, FilePath path_,
	long size_, FILE *fp_, UniMode unicodeMode_, bool visibleProgress_) : 
	FileWorker(pListener_, path_, size_, fp_), documentBytes(documentBytes_), writtenSoFar(0),
		unicodeMode(unicodeMode_), visibleProgress(visibleProgress_) {
	jobSize = static_cast<int>(size);
	jobProgress = 0;
}

FileStorer::~FileStorer() {
}

static bool IsUTF8TrailByte(int ch) {
	return (ch >= 0x80) && (ch < (0x80 + 0x40));
}

void FileStorer::Execute() {
	if (fp) {
		Utf8_16_Write convert;
		if (unicodeMode != uniCookie) {	// Save file with cookie without BOM.
			convert.setEncoding(static_cast<Utf8_16::encodingType>(
					static_cast<int>(unicodeMode)));
		}
		convert.setfile(fp);
		std::vector<char> data(blockSize + 1);
		int lengthDoc = static_cast<int>(size);
		int grabSize;
		for (int i = 0; i < lengthDoc && (!cancelling); i += grabSize) {
#ifdef __unix__
			usleep(sleepTime * 1000);
#else
			::Sleep(sleepTime);
#endif
			grabSize = lengthDoc - i;
			if (grabSize > blockSize)
				grabSize = blockSize;
			if ((unicodeMode != uni8Bit) && (i + grabSize < lengthDoc)) {
				// Round down so only whole characters retrieved.
				int startLast = grabSize;
				while ((startLast > 0) && ((grabSize - startLast) < 6) && IsUTF8TrailByte(static_cast<unsigned char>(documentBytes[i + startLast])))
					startLast--;
				if ((grabSize - startLast) < 5)
					grabSize = startLast;
			}
			memcpy(&data[0], documentBytes+i, grabSize);
			size_t written = convert.fwrite(&data[0], grabSize);
			jobProgress += grabSize;
			if (et.Duration() > nextProgress) {
				nextProgress = et.Duration() + timeBetweenProgress;
				pListener->PostOnMainThread(WORK_FILEPROGRESS, this);
			}
			if (written == 0) {
				err = 1;
				break;
			}
		}
		convert.fclose();
	}
	completed = true;
	pListener->PostOnMainThread(WORK_FILEWRITTEN, this);
}

void FileStorer::Cancel() {
	FileWorker::Cancel();
}
