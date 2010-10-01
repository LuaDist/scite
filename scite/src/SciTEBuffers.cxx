// SciTE - Scintilla based Text Editor
/** @file SciTEBuffers.cxx
 ** Buffers and jobs management.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <string>
#include <map>

#if defined(GTK)

#include <unistd.h>
#include <gtk/gtk.h>

#else

#ifdef __BORLANDC__
// Borland includes Windows.h for STL and defaults to different API number
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#endif

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
#include <commctrl.h>

// For chdir
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif
#ifdef __DMC__
#include <dir.h>
#endif

#endif

#include "Scintilla.h"
#include "SciLexer.h"

#include "GUI.h"

#include "SString.h"
#include "StringList.h"
#include "FilePath.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "SciTEBase.h"

const GUI::gui_char defaultSessionFileName[] = GUI_TEXT("SciTE.session");

BufferList::BufferList() : current(0), stackcurrent(0), stack(0), buffers(0), size(0), length(0), initialised(false) {}

BufferList::~BufferList() {
	delete []buffers;
	delete []stack;
}

void BufferList::Allocate(int maxSize) {
	length = 1;
	current = 0;
	size = maxSize;
	buffers = new Buffer[size];
	stack = new int[size];
	stack[0] = 0;
}

int BufferList::Add() {
	if (length < size) {
		length++;
	}
	buffers[length - 1].Init();
	stack[length - 1] = length - 1;
	MoveToStackTop(length - 1);

	return length - 1;
}

int BufferList::GetDocumentByName(FilePath filename, bool excludeCurrent) {
	if (!filename.IsSet()) {
		return -1;
	}
	for (int i = 0;i < length;i++) {
		if ((!excludeCurrent || i != current) && buffers[i].SameNameAs(filename)) {
			return i;
		}
	}
	return -1;
}

void BufferList::RemoveCurrent() {
	// Delete and move up to fill gap but ensure doc pointer is saved.
	sptr_t currentDoc = buffers[current].doc;
	for (int i = current;i < length - 1;i++) {
		buffers[i] = buffers[i + 1];
	}
	buffers[length - 1].doc = currentDoc;

	if (length > 1) {
		CommitStackSelection();
		PopStack();
		length--;

		buffers[length].Init();
		if (current >= length) {
			SetCurrent(length - 1);
		}
		if (current < 0) {
			SetCurrent(0);
		}
	} else {
		buffers[current].Init();
	}
	MoveToStackTop(current);
}

int BufferList::Current() const {
	return current;
}

Buffer *BufferList::CurrentBuffer() {
	return &buffers[Current()];
}

void BufferList::SetCurrent(int index) {
	current = index;
}

void BufferList::PopStack() {
	for (int i = 0; i < length - 1; ++i) {
		int index = stack[i + 1];
		// adjust the index for items that will move in buffers[]
		if (index > current)
			--index;
		stack[i] = index;
	}
}

int BufferList::StackNext() {
	if (++stackcurrent >= length)
		stackcurrent = 0;
	return stack[stackcurrent];
}

int BufferList::StackPrev() {
	if (--stackcurrent < 0)
		stackcurrent = length - 1;
	return stack[stackcurrent];
}

void BufferList::MoveToStackTop(int index) {
	// shift top chunk of stack down into the slot that index occupies
	bool move = false;
	for (int i = length - 1; i > 0; --i) {
		if (stack[i] == index)
			move = true;
		if (move)
			stack[i] = stack[i-1];
	}
	stack[0] = index;
}

void BufferList::CommitStackSelection() {
	// called only when ctrl key is released when ctrl-tabbing
	// or when a document is closed (in case of Ctrl+F4 during ctrl-tabbing)
	MoveToStackTop(stack[stackcurrent]);
	stackcurrent = 0;
}

void BufferList::ShiftTo(int indexFrom, int indexTo) {
	// shift buffer to new place in buffers array
	if (indexFrom == indexTo ||
		indexFrom < 0 || indexFrom >= length ||
		indexTo < 0 || indexTo >= length) return;
	int step = (indexFrom > indexTo) ? -1 : 1;
	Buffer tmp = buffers[indexFrom];
	int i;
	for (i = indexFrom; i != indexTo; i += step) {
		buffers[i] = buffers[i+step];
	}
	buffers[indexTo] = tmp;
	// update stack indexes
	for (i = 0; i < length; i++) {
		if (stack[i] == indexFrom) {
			stack[i] = indexTo;
		} else if (step == 1) {
			if (indexFrom < stack[i] && stack[i] <= indexTo) stack[i] -= step;
		} else {
			if (indexFrom > stack[i] && stack[i] >= indexTo) stack[i] -= step;
		}
	}
}

sptr_t SciTEBase::GetDocumentAt(int index) {
	if (index < 0 || index >= buffers.size) {
		return 0;
	}
	if (buffers.buffers[index].doc == 0) {
		// Create a new document buffer
		buffers.buffers[index].doc = wEditor.Call(SCI_CREATEDOCUMENT, 0, 0);
	}
	return buffers.buffers[index].doc;
}

void SciTEBase::SetDocumentAt(int index, bool updateStack) {
	int currentbuf = buffers.Current();

	if (	index < 0 ||
	        index >= buffers.length ||
	        index == currentbuf ||
	        currentbuf < 0 ||
	        currentbuf >= buffers.length) {
		return;
	}
	UpdateBuffersCurrent();

	buffers.SetCurrent(index);
	if (updateStack) {
		buffers.MoveToStackTop(index);
	}

	if (extender) {
		if (buffers.size > 1)
			extender->ActivateBuffer(index);
		else
			extender->InitBuffer(0);
	}

	Buffer bufferNext = buffers.buffers[buffers.Current()];
	SetFileName(bufferNext);
	wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
	RestoreState(bufferNext);

	TabSelect(index);

	if (lineNumbers && lineNumbersExpand)
		SetLineNumberWidth();

	DisplayAround(bufferNext);

	CheckMenus();
	UpdateStatusBar(true);

	if (extender) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}
}

void SciTEBase::UpdateBuffersCurrent() {
	int currentbuf = buffers.Current();

	if ((buffers.length > 0) && (currentbuf >= 0)) {
		buffers.buffers[currentbuf].Set(filePath);
		buffers.buffers[currentbuf].selection = GetSelection();
		buffers.buffers[currentbuf].scrollPosition = GetCurrentScrollPosition();

		// Retrieve fold state and store in buffer state info
		int maxLine = wEditor.Call(SCI_GETLINECOUNT);
		int foldPoints = 0;

		for (int line = 0; line < maxLine; line++) {
			if ((wEditor.Call(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG) &&
				!wEditor.Call(SCI_GETFOLDEXPANDED, line)) {
				foldPoints++;
			}
		}

		FoldState *f = &buffers.buffers[currentbuf].foldState;
		f->Clear();

		if (foldPoints > 0) {

			f->Alloc(foldPoints);

			for (int line = 0; line < maxLine; line++) {
				if ((wEditor.Call(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG) &&
					!wEditor.Call(SCI_GETFOLDEXPANDED, line)) {
					f->Append(line);
				}
			}
		}
	}
}

bool SciTEBase::IsBufferAvailable() {
	return buffers.size > 1 && buffers.length < buffers.size;
}

bool SciTEBase::CanMakeRoom(bool maySaveIfDirty) {
	if (IsBufferAvailable()) {
		return true;
	} else if (maySaveIfDirty) {
		// All available buffers are taken, try and close the current one
		if (SaveIfUnsure(true) != IDCANCEL) {
			// The file isn't dirty, or the user agreed to close the current one
			return true;
		}
	} else {
		return true;	// Told not to save so must be OK.
	}
	return false;
}

void SciTEBase::ClearDocument() {
	wEditor.Call(SCI_SETREADONLY, 0);
	wEditor.Call(SCI_CLEARALL);
	wEditor.Call(SCI_EMPTYUNDOBUFFER);
	wEditor.Call(SCI_SETSAVEPOINT);
	wEditor.Call(SCI_SETREADONLY, isReadOnly);
}

void SciTEBase::CreateBuffers() {
	int buffersWanted = props.GetInt("buffers");
	if (buffersWanted > bufferMax) {
		buffersWanted = bufferMax;
	}
	if (buffersWanted < 1) {
		buffersWanted = 1;
	}
	buffers.Allocate(buffersWanted);
}

void SciTEBase::InitialiseBuffers() {
	if (!buffers.initialised) {
		buffers.initialised = true;
		// First document is the default from creation of control
		buffers.buffers[0].doc = wEditor.Call(SCI_GETDOCPOINTER, 0, 0);
		wEditor.Call(SCI_ADDREFDOCUMENT, 0, buffers.buffers[0].doc); // We own this reference
		if (buffers.size == 1) {
			// Single buffer mode, delete the Buffers main menu entry
			DestroyMenuItem(menuBuffers, 0);
			// Destroy command "View Tab Bar" in the menu "View"
			DestroyMenuItem(menuView, IDM_VIEWTABBAR);
			// Make previous change visible.
			RedrawMenu();
		}
	}
}

FilePath SciTEBase::UserFilePath(const GUI::gui_char *name) {
	GUI::gui_string nameWithVisibility(configFileVisibilityString);
	nameWithVisibility += name;
	return FilePath(GetSciteUserHome(), nameWithVisibility.c_str());
}

static SString IndexPropKey(const char *bufPrefix, int bufIndex, const char *bufAppendix) {
	SString pKey = bufPrefix;
	pKey += '.';
	pKey += SString(bufIndex + 1);
	if (bufAppendix != NULL) {
		pKey += ".";
		pKey += bufAppendix;
	}
	return pKey;
}

void SciTEBase::LoadSessionFile(const GUI::gui_char *sessionName) {
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
	} else {
		sessionPathName.Set(sessionName);
	}

	propsSession.Clear();
	propsSession.Read(sessionPathName, sessionPathName.Directory(), NULL, 0);

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::RestoreRecentMenu() {
	Sci_CharacterRange cr;
	cr.cpMin = cr.cpMax = 0;

	DeleteFileStackMenu();

	for (int i = 0; i < fileStackMax; i++) {
		SString propKey = IndexPropKey("mru", i, "path");
		SString propStr = propsSession.Get(propKey.c_str());
		if (propStr == "")
			continue;
		AddFileToStack(GUI::StringFromUTF8(propStr.c_str()), cr, 0);
	}
}

void SciTEBase::RestoreSession() {
	// Comment next line if you don't want to close all buffers before restoring session
	CloseAllBuffers(true);

	int curr = -1;
	for (int i = 0; i < bufferMax; i++) {
		SString propKey = IndexPropKey("buffer", i, "path");
		SString propStr = propsSession.Get(propKey.c_str());
		if (propStr == "")
			continue;

		propKey = IndexPropKey("buffer", i, "position");
		int pos = propsSession.GetInt(propKey.c_str());

		if (!AddFileToBuffer(GUI::StringFromUTF8(propStr.c_str()), pos - 1))
			continue;

		propKey = IndexPropKey("buffer", i, "current");
		if (propsSession.GetInt(propKey.c_str()))
			curr = i;

		if (props.GetInt("session.bookmarks")) {
			propKey = IndexPropKey("buffer", i, "bookmarks");
			propStr = propsSession.Get(propKey.c_str());
			if (propStr.length()) {
				char *buf = new char[propStr.length() + 1];
				strcpy(buf, propStr.c_str());
				char *p = strtok(buf, ",");
				while (p != NULL) {
					int line = atoi(p) - 1;
					wEditor.Call(SCI_MARKERADD, line, markerBookmark);
					p = strtok(NULL, ",");
				}
				delete []buf;
			}
		}

		if (props.GetInt("fold") && !props.GetInt("fold.on.open") &&
			props.GetInt("session.folds")) {
			propKey = IndexPropKey("buffer", i, "folds");
			propStr = propsSession.Get(propKey.c_str());
			if (propStr.length()) {
				wEditor.Call(SCI_COLOURISE, 0, -1);
				char *buf = new char[propStr.length() + 1];
				strcpy(buf, propStr.c_str());
				char *p = strtok(buf, ",");
				while (p != NULL) {
					int line = atoi(p) - 1;
					wEditor.Call(SCI_TOGGLEFOLD, line);
					p = strtok(NULL, ",");
				}
				delete [] buf;
			}
		}

		wEditor.Call(SCI_SCROLLCARET);
	}

	if (curr != -1)
		SetDocumentAt(curr);
}

void SciTEBase::SaveSessionFile(const GUI::gui_char *sessionName) {
	bool defaultSession;
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
		defaultSession = true;
	} else {
		sessionPathName.Set(sessionName);
		defaultSession = false;
	}
	FILE *sessionFile = sessionPathName.Open(fileWrite);
	if (!sessionFile)
		return;

	fprintf(sessionFile, "# SciTE session file\n");

	if (defaultSession && props.GetInt("save.position")) {
		int top, left, width, height, maximize;
		GetWindowPosition(&left, &top, &width, &height, &maximize);

		fprintf(sessionFile, "\n");
		fprintf(sessionFile, "position.left=%d\n", left);
		fprintf(sessionFile, "position.top=%d\n", top);
		fprintf(sessionFile, "position.width=%d\n", width);
		fprintf(sessionFile, "position.height=%d\n", height);
		fprintf(sessionFile, "position.maximize=%d\n", maximize);
	}

	if (defaultSession && props.GetInt("save.recent")) {
		SString propKey;
		int j = 0;

		fprintf(sessionFile, "\n");

		// Save recent files list
		for (int i = fileStackMax - 1; i >= 0; i--) {
			if (recentFileStack[i].IsSet()) {
				propKey = IndexPropKey("mru", j++, "path");
				fprintf(sessionFile, "%s=%s\n", propKey.c_str(), recentFileStack[i].AsUTF8().c_str());
			}
		}
	}

	if (props.GetInt("buffers") && (!defaultSession || props.GetInt("save.session"))) {
		int curr = buffers.Current();
		for (int i = 0; i < buffers.length; i++) {
			if (buffers.buffers[i].IsSet() && !buffers.buffers[i].IsUntitled()) {
				SString propKey = IndexPropKey("buffer", i, "path");
				fprintf(sessionFile, "\n%s=%s\n", propKey.c_str(), buffers.buffers[i].AsUTF8().c_str());

				SetDocumentAt(i);
				int pos = wEditor.Call(SCI_GETCURRENTPOS) + 1;
				propKey = IndexPropKey("buffer", i, "position");
				fprintf(sessionFile, "%s=%d\n", propKey.c_str(), pos);

				if (i == curr) {
					propKey = IndexPropKey("buffer", i, "current");
					fprintf(sessionFile, "%s=1\n", propKey.c_str());
				}

				if (props.GetInt("session.bookmarks")) {
					int line = -1;
					bool found = false;
					while ((line = wEditor.Call(SCI_MARKERNEXT, line + 1, 1 << markerBookmark)) >= 0) {
						if (!found) {
							propKey = IndexPropKey("buffer", i, "bookmarks");
							fprintf(sessionFile, "%s=%d", propKey.c_str(), line + 1);
							found = true;
						} else {
							fprintf(sessionFile, ",%d", line + 1);
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}

				if (props.GetInt("fold") && props.GetInt("session.folds")) {
					int maxLine = wEditor.Call(SCI_GETLINECOUNT);
					bool found = false;
					for (int line = 0; line < maxLine; line++) {
						if ((wEditor.Call(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG) &&
							!wEditor.Call(SCI_GETFOLDEXPANDED, line)) {
							if (!found) {
								propKey = IndexPropKey("buffer", i, "folds");
								fprintf(sessionFile, "%s=%d", propKey.c_str(), line + 1);
								found = true;
							} else {
								fprintf(sessionFile, ",%d", line + 1);
							}
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}
			}
		}
		SetDocumentAt(curr);
	}

	fclose(sessionFile);

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::SetIndentSettings() {
	// Get default values
	int useTabs = props.GetInt("use.tabs", 1);
	int tabSize = props.GetInt("tabsize");
	int indentSize = props.GetInt("indent.size");
	// Either set the settings related to the extension or the default ones
	SString fileNameForExtension = ExtensionFileName();
	SString useTabsChars = props.GetNewExpand("use.tabs.",
	        fileNameForExtension.c_str());
	if (useTabsChars.length() != 0) {
		wEditor.Call(SCI_SETUSETABS, useTabsChars.value());
	} else {
		wEditor.Call(SCI_SETUSETABS, useTabs);
	}
	SString tabSizeForExt = props.GetNewExpand("tab.size.",
	        fileNameForExtension.c_str());
	if (tabSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETTABWIDTH, tabSizeForExt.value());
	} else if (tabSize != 0) {
		wEditor.Call(SCI_SETTABWIDTH, tabSize);
	}
	SString indentSizeForExt = props.GetNewExpand("indent.size.",
	        fileNameForExtension.c_str());
	if (indentSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETINDENT, indentSizeForExt.value());
	} else {
		wEditor.Call(SCI_SETINDENT, indentSize);
	}
}

void SciTEBase::SetEol() {
	SString eol_mode = props.Get("eol.mode");
	if (eol_mode == "LF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode == "CR") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode == "CRLF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CRLF);
	}
}

void SciTEBase::New() {
	InitialiseBuffers();
	UpdateBuffersCurrent();

	if ((buffers.size == 1) && (!buffers.buffers[0].IsUntitled())) {
		AddFileToStack(buffers.buffers[0],
		        buffers.buffers[0].selection,
		        buffers.buffers[0].scrollPosition);
	}

	// If the current buffer is the initial untitled, clean buffer then overwrite it,
	// otherwise add a new buffer.
	if ((buffers.length > 1) ||
	        (buffers.Current() != 0) ||
	        (buffers.buffers[0].isDirty) ||
	        (!buffers.buffers[0].IsUntitled())) {
		if (buffers.size == buffers.length) {
			Close(false, false, true);
		}
		buffers.SetCurrent(buffers.Add());
	}

	sptr_t doc = GetDocumentAt(buffers.Current());
	wEditor.Call(SCI_SETDOCPOINTER, 0, doc);

	FilePath curDirectory(filePath.Directory());
	filePath.Set(curDirectory, GUI_TEXT(""));
	SetFileName(filePath);
	CurrentBuffer()->isDirty = false;
	jobQueue.isBuilding = false;
	jobQueue.isBuilt = false;
	isReadOnly = false;	// No sense to create an empty, read-only buffer...

	ClearDocument();
	DeleteFileStackMenu();
	SetFileStackMenu();
	if (extender)
		extender->InitBuffer(buffers.Current());
}

void SciTEBase::RestoreState(const Buffer &buffer) {
	SetWindowName();
	ReadProperties();
	if (CurrentBuffer()->unicodeMode != uni8Bit) {
		// Override the code page if Unicode
		codePage = SC_CP_UTF8;
		wEditor.Call(SCI_SETCODEPAGE, codePage);
	}
	isReadOnly = wEditor.Call(SCI_GETREADONLY);

	// check to see whether there is saved fold state, restore
	for (int fold = 0; fold < buffer.foldState.Folds(); fold++) {
		wEditor.Call(SCI_TOGGLEFOLD, buffer.foldState.Line(fold));
	}
}

void SciTEBase::Close(bool updateUI, bool loadingSession, bool makingRoomForNew) {
	bool closingLast = false;

	if (extender) {
		extender->OnClose(filePath.AsUTF8().c_str());
	}

	if (buffers.size == 1) {
		// With no buffer list, Close means close from MRU
		closingLast = !(recentFileStack[0].IsSet());
		buffers.buffers[0].Init();
		filePath.Set(GUI_TEXT(""));
		ClearDocument(); //avoid double are-you-sure
		if (!makingRoomForNew)
			StackMenu(0); // calls New, or Open, which calls InitBuffer
	} else if (buffers.size > 1) {
		if (buffers.Current() >= 0 && buffers.Current() < buffers.length) {
			UpdateBuffersCurrent();
			Buffer buff = buffers.buffers[buffers.Current()];
			AddFileToStack(buff, buff.selection, buff.scrollPosition);
		}
		closingLast = (buffers.length == 1);
		if (closingLast) {
			buffers.buffers[0].Init();
			if (extender)
				extender->InitBuffer(0);
		} else {
			if (extender)
				extender->RemoveBuffer(buffers.Current());
			ClearDocument();
			buffers.RemoveCurrent();
			if (extender && !makingRoomForNew)
				extender->ActivateBuffer(buffers.Current());
		}
		Buffer bufferNext = buffers.buffers[buffers.Current()];

		if (updateUI)
			SetFileName(bufferNext);
		wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
		if (closingLast) {
			ClearDocument();
		}
		if (updateUI)
			CheckReload();
		if (updateUI) {
			RestoreState(bufferNext);
			DisplayAround(bufferNext);
		}
	}

	if (updateUI) {
		BuffersMenu();
		UpdateStatusBar(true);
	}

	if (extender && !closingLast && !makingRoomForNew) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}

	if (closingLast && props.GetInt("quit.on.close.last") && !loadingSession) {
		QuitProgram();
	}
}

void SciTEBase::CloseTab(int tab) {
	int tabCurrent = buffers.Current();
	if (tab == tabCurrent) {
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
		}
	} else {
		FilePath fpCurrent = buffers.buffers[tabCurrent].AbsolutePath();
		SetDocumentAt(tab);
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			WindowSetFocus(wEditor);
			// Return to the previous buffer
			SetDocumentAt(buffers.GetDocumentByName(fpCurrent));
		}
	}
}

void SciTEBase::CloseAllBuffers(bool loadingSession) {
	if (SaveAllBuffers(false) != IDCANCEL) {
		while (buffers.length > 1)
			Close(false, loadingSession);

		Close(true, loadingSession);
	}
}

int SciTEBase::SaveAllBuffers(bool forceQuestion, bool alwaysYes) {
	int choice = IDYES;
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; (i < buffers.length) && (choice != IDCANCEL); i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (alwaysYes) {
				if (!Save()) {
					choice = IDCANCEL;
				}
			} else {
				choice = SaveIfUnsure(forceQuestion);
			}
		}
	}
	SetDocumentAt(currentBuffer);
	return choice;
}

void SciTEBase::SaveTitledBuffers() {
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; i < buffers.length; i++) {
		if (buffers.buffers[i].isDirty && !buffers.buffers[i].IsUntitled()) {
			SetDocumentAt(i);
			Save();
		}
	}
	SetDocumentAt(currentBuffer);
}

void SciTEBase::Next() {
	int next = buffers.Current();
	if (++next >= buffers.length)
		next = 0;
	SetDocumentAt(next);
	CheckReload();
}

void SciTEBase::Prev() {
	int prev = buffers.Current();
	if (--prev < 0)
		prev = buffers.length - 1;

	SetDocumentAt(prev);
	CheckReload();
}

void SciTEBase::ShiftTab(int indexFrom, int indexTo) {
	buffers.ShiftTo(indexFrom, indexTo);
	buffers.SetCurrent(indexTo);
	BuffersMenu();

	TabSelect(indexTo);

	DisplayAround(buffers.buffers[buffers.Current()]);
}

void SciTEBase::MoveTabRight() {
	if (buffers.length < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom + 1;
	if (indexTo >= buffers.length) indexTo = 0;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::MoveTabLeft() {
	if (buffers.length < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom - 1;
	if (indexTo < 0) indexTo = buffers.length - 1;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::NextInStack() {
	SetDocumentAt(buffers.StackNext(), false);
	CheckReload();
}

void SciTEBase::PrevInStack() {
	SetDocumentAt(buffers.StackPrev(), false);
	CheckReload();
}

void SciTEBase::EndStackedTabbing() {
	buffers.CommitStackSelection();
}

void SciTEBase::BuffersMenu() {
	UpdateBuffersCurrent();
	DestroyMenuItem(menuBuffers, IDM_BUFFERSEP);
	RemoveAllTabs();

	int pos;
	for (pos = 0; pos < bufferMax; pos++) {
		DestroyMenuItem(menuBuffers, IDM_BUFFER + pos);
	}
	if (buffers.size > 1) {
		int menuStart = 5;
		SetMenuItem(menuBuffers, menuStart, IDM_BUFFERSEP, GUI_TEXT(""));
		for (pos = 0; pos < buffers.length; pos++) {
			int itemID = bufferCmdID + pos;
			GUI::gui_string entry;
			GUI::gui_string titleTab;
#if !defined(GTK)

			if (pos < 10) {
				GUI::gui_char stemp[100];
#if defined(_MSC_VER) && (_MSC_VER > 1200)
				swprintf(stemp, ELEMENTS(stemp), GUI_TEXT("&%d "), (pos + 1) % 10 );
#else
				swprintf(stemp, GUI_TEXT("&%d "), (pos + 1) % 10 );
#endif
				entry = stemp;	// hotkey 1..0
				titleTab = stemp; // add hotkey to the tabbar
			}
#endif

			if (buffers.buffers[pos].IsUntitled()) {
				GUI::gui_string untitled = localiser.Text("Untitled");
				entry += untitled;
				titleTab += untitled;
			} else {
				GUI::gui_string path = buffers.buffers[pos].AsInternal();
#if !defined(GTK)
				// Handle '&' characters in path, since they are interpreted in
				// menues and tab names.
				size_t amp = 0;
				while ((amp = path.find(GUI_TEXT("&"), amp)) != GUI::gui_string::npos) {
					path.insert(amp, GUI_TEXT("&"));
					amp += 2;
				}
#endif
				entry += path;

				size_t dirEnd = entry.rfind(pathSepChar);
				if (dirEnd != GUI::gui_string::npos) {
					titleTab += entry.substr(dirEnd + 1);
				} else {
					titleTab += entry;
				}
			}
			// For short file names:
			//char *cpDirEnd = strrchr(buffers.buffers[pos]->fileName, pathSepChar);
			//strcat(entry, cpDirEnd + 1);

			if (buffers.buffers[pos].isDirty) {
				entry += GUI_TEXT(" *");
				titleTab += GUI_TEXT(" *");
			}

			SetMenuItem(menuBuffers, menuStart + pos + 1, itemID, entry.c_str());
			TabInsert(pos, titleTab.c_str());
		}
	}
	CheckMenus();
#if !defined(GTK)

	if (tabVisible)
		SizeSubWindows();
#endif
#if defined(GTK)
	ShowTabBar();
#endif
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(menuFile, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(menuFile, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	if (recentFileStack[0].IsSet()) {
		SetMenuItem(menuFile, MRU_START, IDM_MRU_SEP, GUI_TEXT(""));
		for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].IsSet()) {
				GUI::gui_char entry[MAX_PATH + 20];
				entry[0] = '\0';
#if !defined(GTK)

#if defined(_MSC_VER) && (_MSC_VER > 1200)
				swprintf(entry, ELEMENTS(entry), GUI_TEXT("&%d "), (stackPos + 1) % 10);
#else
				swprintf(entry, GUI_TEXT("&%d "), (stackPos + 1) % 10);
#endif
#endif
				GUI::gui_string sEntry(entry);
				sEntry += recentFileStack[stackPos].AsInternal();
				SetMenuItem(menuFile, MRU_START + stackPos + 1, itemID, sEntry.c_str());
			}
		}
	}
}

void SciTEBase::DropFileStackTop() {
	DeleteFileStackMenu();
	for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++)
		recentFileStack[stackPos] = recentFileStack[stackPos + 1];
	recentFileStack[fileStackMax - 1].Init();
	SetFileStackMenu();
}

bool SciTEBase::AddFileToBuffer(FilePath file, int pos) {
	// Return whether file loads successfully
	if (file.Exists() && Open(file, ofForceLoad)) {
		wEditor.Call(SCI_GOTOPOS, pos);
		return true;
	} else {
		return false;
	}
}

void SciTEBase::AddFileToStack(FilePath file, Sci_CharacterRange selection, int scrollPos) {
	if (!file.IsSet())
		return;
	DeleteFileStackMenu();
	// Only stack non-empty names
	if (file.IsSet() && !file.IsUntitled()) {
		int stackPos;
		int eqPos = fileStackMax - 1;
		for (stackPos = 0; stackPos < fileStackMax; stackPos++)
			if (recentFileStack[stackPos].SameNameAs(file))
				eqPos = stackPos;
		for (stackPos = eqPos; stackPos > 0; stackPos--)
			recentFileStack[stackPos] = recentFileStack[stackPos - 1];
		recentFileStack[0].Set(file);
		recentFileStack[0].selection = selection;
		recentFileStack[0].scrollPosition = scrollPos;
	}
	SetFileStackMenu();
}

void SciTEBase::RemoveFileFromStack(FilePath file) {
	if (!file.IsSet())
		return;
	DeleteFileStackMenu();
	int stackPos;
	for (stackPos = 0; stackPos < fileStackMax; stackPos++) {
		if (recentFileStack[stackPos].SameNameAs(file)) {
			for (int movePos = stackPos; movePos < fileStackMax - 1; movePos++)
				recentFileStack[movePos] = recentFileStack[movePos + 1];
			recentFileStack[fileStackMax - 1].Init();
			break;
		}
	}
	SetFileStackMenu();
}

RecentFile SciTEBase::GetFilePosition() {
	RecentFile rf;
	rf.selection = GetSelection();
	rf.scrollPosition = GetCurrentScrollPosition();
	return rf;
}

void SciTEBase::DisplayAround(const RecentFile &rf) {
	if ((rf.selection.cpMin != INVALID_POSITION) && (rf.selection.cpMax != INVALID_POSITION)) {
		int lineStart = wEditor.Call(SCI_LINEFROMPOSITION, rf.selection.cpMin);
		wEditor.Call(SCI_ENSUREVISIBLEENFORCEPOLICY, lineStart);
		int lineEnd = wEditor.Call(SCI_LINEFROMPOSITION, rf.selection.cpMax);
		wEditor.Call(SCI_ENSUREVISIBLEENFORCEPOLICY, lineEnd);
		SetSelection(rf.selection.cpMax, rf.selection.cpMin);

		int curTop = wEditor.Call(SCI_GETFIRSTVISIBLELINE);
		int lineTop = wEditor.Call(SCI_VISIBLEFROMDOCLINE, rf.scrollPosition);
		wEditor.Call(SCI_LINESCROLL, 0, lineTop - curTop);
	}
}

// Next and Prev file comments.
// Decided that "Prev" file should mean the file you had opened last
// This means "Next" file means the file you opened longest ago.
void SciTEBase::StackMenuNext() {
	DeleteFileStackMenu();
	for (int stackPos = fileStackMax - 1; stackPos >= 0;stackPos--) {
		if (recentFileStack[stackPos].IsSet()) {
			SetFileStackMenu();
			StackMenu(stackPos);
			return;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::StackMenuPrev() {
	if (recentFileStack[0].IsSet()) {
		// May need to restore last entry if removed by StackMenu
		RecentFile rfLast = recentFileStack[fileStackMax - 1];
		StackMenu(0);	// Swap current with top of stack
		for (int checkPos = 0; checkPos < fileStackMax; checkPos++) {
			if (rfLast.SameNameAs(recentFileStack[checkPos])) {
				rfLast.Init();
			}
		}
		// And rotate the MRU
		RecentFile rfCurrent = recentFileStack[0];
		// Move them up
		for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++) {
			recentFileStack[stackPos] = recentFileStack[stackPos + 1];
		}
		recentFileStack[fileStackMax - 1].Init();
		// Copy current file into first empty
		for (int emptyPos = 0; emptyPos < fileStackMax; emptyPos++) {
			if (!recentFileStack[emptyPos].IsSet()) {
				if (rfLast.IsSet()) {
					recentFileStack[emptyPos] = rfLast;
					rfLast.Init();
				} else {
					recentFileStack[emptyPos] = rfCurrent;
					break;
				}
			}
		}

		DeleteFileStackMenu();
		SetFileStackMenu();
	}
}

void SciTEBase::StackMenu(int pos) {
	if (CanMakeRoom(true)) {
		if (pos >= 0) {
			if ((pos == 0) && (!recentFileStack[pos].IsSet())) {	// Empty
				New();
				SetWindowName();
				ReadProperties();
				SetIndentSettings();
			} else if (recentFileStack[pos].IsSet()) {
				RecentFile rf = recentFileStack[pos];
				// Already asked user so don't allow Open to ask again.
				Open(rf, ofNoSaveIfDirty);
				DisplayAround(rf);
			}
		}
	}
}

void SciTEBase::RemoveToolsMenu() {
	for (int pos = 0; pos < toolMax; pos++) {
		DestroyMenuItem(menuTools, IDM_TOOLS + pos);
	}
}

void SciTEBase::SetMenuItemLocalised(int menuNumber, int position, int itemID,
        const char *text, const char *mnemonic) {
	GUI::gui_string localised = localiser.Text(text);
	SetMenuItem(menuNumber, position, itemID, localised.c_str(), GUI::StringFromUTF8(mnemonic).c_str());
}

void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	int menuPos = TOOLS_START;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		SString prefix = "command.name.";
		prefix += SString(item);
		prefix += ".";
		SString commandName = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
		if (commandName.length()) {
			SString sMenuItem = commandName;
			prefix = "command.shortcut.";
			prefix += SString(item);
			prefix += ".";
			SString sMnemonic = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
			if (item < 10 && sMnemonic.length() == 0) {
				sMnemonic += "Ctrl+";
				sMnemonic += SString(item);
			}
			SetMenuItemLocalised(menuTools, menuPos, itemID, sMenuItem.c_str(),
				sMnemonic[0] ? sMnemonic.c_str() : NULL);
			menuPos++;
		}
	}

	DestroyMenuItem(menuTools, IDM_MACRO_SEP);
	DestroyMenuItem(menuTools, IDM_MACROLIST);
	DestroyMenuItem(menuTools, IDM_MACROPLAY);
	DestroyMenuItem(menuTools, IDM_MACRORECORD);
	DestroyMenuItem(menuTools, IDM_MACROSTOPRECORD);
	menuPos++;
	if (macrosEnabled) {
		SetMenuItem(menuTools, menuPos++, IDM_MACRO_SEP, GUI_TEXT(""));
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROLIST,
		        "&List Macros...", "Shift+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROPLAY,
		        "Run Current &Macro", "F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACRORECORD,
		        "&Record Macro", "Ctrl+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROSTOPRECORD,
		        "S&top Recording Macro", "Ctrl+Shift+F9");
	}
}

JobSubsystem SciTEBase::SubsystemType(char c) {
	if (c == '1')
		return jobGUI;
	else if (c == '2')
		return jobShell;
	else if (c == '3')
		return jobExtension;
	else if (c == '4')
		return jobHelp;
	else if (c == '5')
		return jobOtherHelp;
	return jobCLI;
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd, int item) {
	SString subsysprefix = cmd;
	if (item >= 0) {
		subsysprefix += SString(item);
		subsysprefix += ".";
	}
	SString subsystem = props.GetNewExpand(subsysprefix.c_str(), FileNameExt().AsUTF8().c_str());
	return SubsystemType(subsystem[0]);
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	SString itemSuffix = item;
	itemSuffix += '.';

	SString propName = "command.";
	propName += itemSuffix;

	SString command = props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str());
	if (command.length()) {
		int saveBefore = 0;

		JobSubsystem jobType = jobCLI;
		bool filter = false;
		bool quiet = false;
		int repSel = 0;
		bool groupUndo = false;

		propName = "command.mode.";
		propName += itemSuffix;
		SString modeVal = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
		modeVal.remove(" ");
		if (modeVal.length()) {
			char *modeTags = modeVal.detach();

			// copy/paste from style selectors.
			char *opt = modeTags;
			while (opt) {
				// Find attribute separator
				char *cpComma = strchr(opt, ',');
				if (cpComma) {
					// If found, we terminate the current attribute (opt) string
					*cpComma = '\0';
				}
				// Find attribute name/value separator
				char *colon = strchr(opt, ':');
				if (colon) {
					// If found, we terminate the current attribute name and point on the value
					*colon++ = '\0';
				}

				if (0 == strcmp(opt, "subsystem") && colon) {
					if (colon[0] == '0' || 0 == strcmp(colon, "console"))
						jobType = jobCLI;
					else if (colon[0] == '1' || 0 == strcmp(colon, "windows"))
						jobType = jobGUI;
					else if (colon[0] == '2' || 0 == strcmp(colon, "shellexec"))
						jobType = jobShell;
					else if (colon[0] == '3' || 0 == strcmp(colon, "lua") || 0 == strcmp(colon, "director"))
						jobType = jobExtension;
					else if (colon[0] == '4' || 0 == strcmp(colon, "htmlhelp"))
						jobType = jobHelp;
					else if (colon[0] == '5' || 0 == strcmp(colon, "winhelp"))
						jobType = jobOtherHelp;
				}

				if (0 == strcmp(opt, "quiet")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						quiet = true;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						quiet = false;
				}

				if (0 == strcmp(opt, "savebefore")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						saveBefore = 1;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						saveBefore = 2;
					else if (0 == strcmp(colon, "prompt"))
						saveBefore = 0;
				}

				if (0 == strcmp(opt, "filter")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						filter = true;
					else if (colon[1] == '0' || 0 == strcmp(colon, "no"))
						filter = false;
				}

				if (0 == strcmp(opt, "replaceselection")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						repSel = 1;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						repSel = 0;
					else if (0 == strcmp(colon, "auto"))
						repSel = 2;
				}

				if (0 == strcmp(opt, "groupundo")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						groupUndo = true;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						groupUndo = false;
				}

				opt = cpComma ? cpComma + 1 : 0;
			}
			delete []modeTags;
		}

		// The mode flags also have classic properties with similar effect.
		// If the classic property is specified, it overrides the mode.
		// To see if the property is absent (as opposed to merely evaluating
		// to nothing after variable expansion), use GetWild for the
		// existence check.  However, for the value check, use getNewExpand.

		propName = "command.save.before.";
		propName += itemSuffix;
		if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
			saveBefore = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value();

		if (saveBefore == 2 || (saveBefore == 1 && (!(CurrentBuffer()->isDirty) || Save())) || SaveIfUnsure() != IDCANCEL) {
			int flags = 0;

			propName = "command.is.filter.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				filter = (props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str())[0] == '1');
			if (filter)
				CurrentBuffer()->fileModTime -= 1;

			propName = "command.subsystem.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length()) {
				SString subsystemVal = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
				jobType = SubsystemType(subsystemVal[0]);
			}

			propName = "command.input.";
			propName += itemSuffix;
			SString input;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length()) {
				input = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
				flags |= jobHasInput;
			}

			propName = "command.quiet.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				quiet = (props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value() == 1);
			if (quiet)
				flags |= jobQuiet;

			propName = "command.replace.selection.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				repSel = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value();

			if (repSel == 1)
				flags |= jobRepSelYes;
			else if (repSel == 2)
				flags |= jobRepSelAuto;

			if (groupUndo)
				flags |= jobGroupUndo;

			AddCommand(command, "", jobType, input, flags);
			if (jobQueue.commandCurrent > 0)
				Execute();
		}
	}
}

inline bool isdigitchar(int ch) {
	return (ch >= '0') && (ch <= '9');
}

int DecodeMessage(const char *cdoc, char *sourcePath, int format, int &column) {
	sourcePath[0] = '\0';
	column = -1; // default to not detected
	switch (format) {
	case SCE_ERR_PYTHON: {
			// Python
			const char *startPath = strchr(cdoc, '\"');
			if (startPath) {
				startPath++;
				const char *endPath = strchr(startPath, '\"');
				if (endPath) {
					int length = endPath - startPath;
					if (length > 0) {
						strncpy(sourcePath, startPath, length);
						sourcePath[length] = 0;
					}
					endPath++;
					while (*endPath && !isdigitchar(*endPath)) {
						endPath++;
					}
					int sourceNumber = atoi(endPath) - 1;
					return sourceNumber;
				}
			}
		}
	case SCE_ERR_GCC: {
			// GCC - look for number after colon to be line number
			// This will be preceded by file name.
			// Lua debug traceback messages also piggyback this style, but begin with a tab.
			if (cdoc[0] == '\t')
				++cdoc;
			for (int i = 0; cdoc[i]; i++) {
				if (cdoc[i] == ':' && isdigitchar(cdoc[i + 1])) {
					int sourceNumber = atoi(cdoc + i + 1) - 1;
					if (i > 0) {
						strncpy(sourcePath, cdoc, i);
						sourcePath[i] = 0;
					}
					return sourceNumber;
				}
			}
			break;
		}
	case SCE_ERR_MS: {
			// Visual *
			const char *start = cdoc;
			while (isspacechar(*start)) {
				start++;
			}
			const char *endPath = strchr(start, '(');
			int length = endPath - start;
			if ((length > 0) && (length < MAX_PATH)) {
				strncpy(sourcePath, start, length);
				sourcePath[length] = 0;
			}
			endPath++;
			return atoi(endPath) - 1;
		}
	case SCE_ERR_BORLAND: {
			// Borland
			const char *space = strchr(cdoc, ' ');
			if (space) {
				while (isspacechar(*space)) {
					space++;
				}
				while (*space && !isspacechar(*space)) {
					space++;
				}
				while (isspacechar(*space)) {
					space++;
				}

				const char *space2 = NULL;

				if (strlen(space) > 2) {
					space2 = strchr(space + 2, ':');
				}

				if (space2) {
					while (!isspacechar(*space2)) {
						space2--;
					}

					while (isspacechar(*(space2 - 1))) {
						space2--;
					}

					int length = space2 - space;

					if (length > 0) {
						strncpy(sourcePath, space, length);
						sourcePath[length] = '\0';
						return atoi(space2) - 1;
					}
				}
			}
			break;
		}
	case SCE_ERR_PERL: {
			// perl
			const char *at = strstr(cdoc, " at ");
			const char *line = strstr(cdoc, " line ");
			int length = line - (at + 4);
			if (at && line && length > 0) {
				strncpy(sourcePath, at + 4, length);
				sourcePath[length] = 0;
				line += 6;
				return atoi(line) - 1;
			}
			break;
		}
	case SCE_ERR_NET: {
			// .NET traceback
			const char *in = strstr(cdoc, " in ");
			const char *line = strstr(cdoc, ":line ");
			if (in && line && (line > in)) {
				in += 4;
				strncpy(sourcePath, in, line - in);
				sourcePath[line - in] = 0;
				line += 6;
				return atoi(line) - 1;
			}
		}
	case SCE_ERR_LUA: {
			// Lua 4 error looks like: last token read: `result' at line 40 in file `Test.lua'
			const char *idLine = "at line ";
			const char *idFile = "file ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file) {
				const char *fileStart = file + lenFile + 1;
				const char *quote = strstr(fileStart, "'");
				size_t length = quote - fileStart;
				if (quote && length > 0) {
					strncpy(sourcePath, fileStart, length);
					sourcePath[length] = '\0';
				}
				line += lenLine;
				return atoi(line) - 1;
			} else {
				// Lua 5.1 error looks like: lua.exe: test1.lua:3: syntax error
				// reuse the GCC error parsing code above!
				const char* colon = strstr(cdoc, ": ");
				if (cdoc)
					return DecodeMessage(colon + 2, sourcePath, SCE_ERR_GCC, column);
			}
			break;
		}

	case SCE_ERR_CTAG: {
			for (int i = 0; cdoc[i]; i++) {
				if ((isdigitchar(cdoc[i + 1]) || (cdoc[i + 1] == '/' && cdoc[i + 2] == '^')) && cdoc[i] == '\t') {
					int j = i - 1;
					while (j > 0 && ! strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j--;
					}
					if (strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j++;
					}
					strncpy(sourcePath, &cdoc[j], i - j);
					sourcePath[i - j] = 0;
					// Because usually the address is a searchPattern, lineNumber has to be evaluated later
					return 0;
				}
			}
		}
	case SCE_ERR_PHP: {
			// PHP error look like: Fatal error: Call to undefined function:  foo() in example.php on line 11
			const char *idLine = " on line ";
			const char *idFile = " in ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_ELF: {
			// Essential Lahey Fortran error look like: Line 11, file c:\fortran90\codigo\demo.f90
			const char *line = strchr(cdoc, ' ');
			if (line) {
				while (isspacechar(*line)) {
					line++;
				}
				const char *file = strchr(line, ' ');
				if (file) {
					while (isspacechar(*file)) {
						file++;
					}
					while (*file && !isspacechar(*file)) {
						file++;
					}
					size_t length = strlen(file);
					strncpy(sourcePath, file, length);
					sourcePath[length] = '\0';
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_IFC: {
			/* Intel Fortran Compiler error/warnings look like:
			 * Error 71 at (17:teste.f90) : The program unit has no name
			 * Warning 4 at (9:modteste.f90) : Tab characters are an extension to standard Fortran 95
			 *
			 * Depending on the option, the error/warning messages can also appear on the form:
			 * modteste.f90(9): Warning 4 : Tab characters are an extension to standard Fortran 95
			 *
			 * These are trapped by the MS handler, and are identified OK, so no problem...
			 */
			const char *line = strchr(cdoc, '(');
			const char *file = strchr(line, ':');
			if (line && file) {
				file++;
				const char *endfile = strchr(file, ')');
				size_t length = endfile - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line++;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_ABSF: {
			// Absoft Pro Fortran 90/95 v8.x, v9.x  errors look like: cf90-113 f90fe: ERROR SHF3D, File = shf.f90, Line = 1101, Column = 19
			const char *idFile = " File = ";
			const char *idLine = ", Line = ";
			const char *idColumn = ", Column = ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			const char *column = strstr(cdoc, idColumn);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				length = column - line;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_IFORT: {
			/* Intel Fortran Compiler v8.x error/warnings look like:
			 * fortcom: Error: shf.f90, line 5602: This name does not have ...
				 */
			const char *idFile = ": Error: ";
			const char *idLine = ", line ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			const char *lineend = strrchr(cdoc, ':');
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				if ((lineend > line)) {
					length = lineend - line;
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_TIDY: {
			/* HTML Tidy error/warnings look like:
			 * line 8 column 1 - Error: unexpected </head> in <meta>
			 * line 41 column 1 - Warning: <table> lacks "summary" attribute
			 */
			const char *line = strchr(cdoc, ' ');
			if (line) {
				const char *col = strchr(line + 1, ' ');
				if (col) {
					//*col = '\0';
					int lnr = atoi(line) - 1;
					col = strchr(col + 1, ' ');
					if (col) {
						const char *endcol = strchr(col + 1, ' ');
						if (endcol) {
							//*endcol = '\0';
							column = atoi(col) - 1;
							return lnr;
						}
					}
				}
			}
			break;
		}

	case SCE_ERR_JAVA_STACK: {
			/* Java runtime stack trace
				\tat <methodname>(<filename>:<line>)
				 */
			const char *startPath = strrchr(cdoc, '(') + 1;
			const char *endPath = strchr(startPath, ':');
			int length = endPath - startPath;
			if (length > 0) {
				strncpy(sourcePath, startPath, length);
				sourcePath[length] = 0;
				int sourceNumber = atoi(endPath + 1) - 1;
				return sourceNumber;
			}
			break;
		}

	case SCE_ERR_DIFF_MESSAGE: {
			// Diff file header, either +++ <filename>\t or --- <filename>\t
			// Often followed by a position line @@ <linenumber>
			const char *startPath = cdoc + 4;
			const char *endPath = strchr(startPath, '\t');
			if (endPath) {
				int length = endPath - startPath;
				strncpy(sourcePath, startPath, length);
				sourcePath[length] = 0;
				return 0;
			}
			break;
		}
	}	// switch
	return -1;
}

void SciTEBase::GoMessage(int dir) {
	Sci_CharacterRange crange;
	crange.cpMin = wOutput.Call(SCI_GETSELECTIONSTART);
	crange.cpMax = wOutput.Call(SCI_GETSELECTIONEND);
	int selStart = crange.cpMin;
	int curLine = wOutput.Call(SCI_LINEFROMPOSITION, selStart);
	int maxLine = wOutput.Call(SCI_GETLINECOUNT);
	int lookLine = curLine + dir;
	if (lookLine < 0)
		lookLine = maxLine - 1;
	else if (lookLine >= maxLine)
		lookLine = 0;
	TextReader acc(wOutput);
	while ((dir == 0) || (lookLine != curLine)) {
		int startPosLine = wOutput.Call(SCI_POSITIONFROMLINE, lookLine, 0);
		int lineLength = wOutput.Call(SCI_LINELENGTH, lookLine, 0);
		char style = acc.StyleAt(startPosLine);
		if (style != SCE_ERR_DEFAULT &&
		        style != SCE_ERR_CMD &&
		        style != SCE_ERR_DIFF_ADDITION &&
		        style != SCE_ERR_DIFF_CHANGED &&
		        style != SCE_ERR_DIFF_DELETION) {
			wOutput.Call(SCI_MARKERDELETEALL, static_cast<uptr_t>(-1));
			wOutput.Call(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			wOutput.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
			        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
			wOutput.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
			        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
			wOutput.Call(SCI_MARKERADD, lookLine, 0);
			wOutput.Call(SCI_SETSEL, startPosLine, startPosLine);
			SString message = GetRange(wOutput, startPosLine, startPosLine + lineLength);
			char source[MAX_PATH];
			int column;
			int sourceLine = DecodeMessage(message.c_str(), source, style, column);
			if (sourceLine >= 0) {
				GUI::gui_string sourceString = GUI::StringFromUTF8(source);
				FilePath sourcePath(sourceString);
				if (!filePath.Name().SameNameAs(sourcePath)) {
					FilePath messagePath;
					bool bExists = false;
					if (Exists(dirNameAtExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(dirNameForExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(filePath.Directory().AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(NULL, sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else {
						// Look through buffers for name match
						for (int i = buffers.length - 1; i >= 0; i--) {
							if (sourcePath.Name().SameNameAs(buffers.buffers[i].Name())) {
								messagePath = buffers.buffers[i];
								bExists = true;
							}
						}
					}
					if (bExists) {
						if (!Open(messagePath)) {
							return;
						}
						CheckReload();
					}
				}

				// If ctag then get line number after search tag or use ctag line number
				if (style == SCE_ERR_CTAG) {
					char cTag[200];
					//without following focus GetCTag wouldn't work correct
					WindowSetFocus(wOutput);
					GetCTag(cTag, 200);
					if (cTag[0] != '\0') {
						if (atol(cTag) > 0) {
							//if tag is linenumber, get line
							sourceLine = atol(cTag) - 1;
						} else {
							findWhat = cTag;
							FindNext(false);
							//get linenumber for marker from found position
							sourceLine = wEditor.Call(SCI_LINEFROMPOSITION, wEditor.Call(SCI_GETCURRENTPOS));
						}
					}
				}

				wEditor.Call(SCI_MARKERDELETEALL, 0);
				wEditor.Call(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				wEditor.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
				        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
				wEditor.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
				        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
				wEditor.Call(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine, 0);
				int endSourceline = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine + 1, 0);
				if (column >= 0) {
					// Get the position in line according to current tab setting
					startSourceLine = wEditor.Call(SCI_FINDCOLUMN, sourceLine, column);
				}
				EnsureRangeVisible(startSourceLine, startSourceLine);
				if (props.GetInt("error.select.line") == 1) {
					//select whole source source line from column with error
					SetSelection(endSourceline, startSourceLine);
				} else {
					//simply move cursor to line, don't do any selection
					SetSelection(startSourceLine, startSourceLine);
				}
				message.substitute('\t', ' ');
				message.remove("\n");
				props.Set("CurrentMessage", message.c_str());
				UpdateStatusBar(false);
				WindowSetFocus(wEditor);
			}
			return;
		}
		lookLine += dir;
		if (lookLine < 0)
			lookLine = maxLine - 1;
		else if (lookLine >= maxLine)
			lookLine = 0;
		if (dir == 0)
			return;
	}
}

