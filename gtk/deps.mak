DirectorExtension.o: DirectorExtension.cxx \
 ../../scintilla/include/Scintilla.h ../../scintilla/include/ILexer.h \
 ../src/GUI.h ../src/SString.h ../src/StringList.h ../src/StringHelpers.h \
 ../src/FilePath.h ../src/PropSetFile.h ../src/Extender.h \
 DirectorExtension.h ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h \
 ../src/Cookie.h ../src/Worker.h ../src/SciTEBase.h
GTKMutex.o: GTKMutex.cxx ../src/Mutex.h
GUIGTK.o: GUIGTK.cxx \
 ../../scintilla/include/Scintilla.h \
 ../../scintilla/include/ScintillaWidget.h ../src/GUI.h
SciTEGTK.o: SciTEGTK.cxx \
 ../../scintilla/include/Scintilla.h \
 ../../scintilla/include/ScintillaWidget.h \
 ../../scintilla/include/ILexer.h ../src/GUI.h ../src/SString.h \
 ../src/StringList.h ../src/StringHelpers.h ../src/FilePath.h \
 ../src/PropSetFile.h ../src/Extender.h ../src/MultiplexExtension.h \
 ../src/Extender.h DirectorExtension.h ../src/LuaExtension.h \
 ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h pixmapsGNOME.h SciIcon.h \
 Widget.h ../src/Cookie.h ../src/Worker.h ../src/SciTEBase.h \
 ../src/SciTEKeys.h
Widget.o: Widget.cxx \
 ../../scintilla/include/Scintilla.h ../src/GUI.h ../src/StringHelpers.h \
 Widget.h
Cookie.o: ../src/Cookie.cxx ../src/SString.h ../src/Cookie.h
Credits.o: ../src/Credits.cxx ../../scintilla/include/Scintilla.h \
 ../../scintilla/include/ILexer.h ../src/GUI.h ../src/SString.h \
 ../src/StringList.h ../src/StringHelpers.h ../src/FilePath.h \
 ../src/PropSetFile.h ../src/StyleWriter.h ../src/Extender.h \
 ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h ../src/Cookie.h \
 ../src/Worker.h ../src/SciTEBase.h
Exporters.o: ../src/Exporters.cxx \
 ../../scintilla/include/Scintilla.h ../../scintilla/include/ILexer.h \
 ../src/GUI.h ../src/SString.h ../src/StringList.h ../src/StringHelpers.h \
 ../src/FilePath.h ../src/PropSetFile.h ../src/StyleWriter.h \
 ../src/Extender.h ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h \
 ../src/Cookie.h ../src/Worker.h ../src/SciTEBase.h
FilePath.o: ../src/FilePath.cxx \
 ../../scintilla/include/Scintilla.h ../src/GUI.h ../src/SString.h \
 ../src/FilePath.h
FileWorker.o: ../src/FileWorker.cxx ../../scintilla/include/Scintilla.h \
 ../../scintilla/include/ILexer.h ../src/GUI.h ../src/SString.h \
 ../src/FilePath.h ../src/Cookie.h ../src/Worker.h ../src/FileWorker.h \
 ../src/Utf8_16.h
IFaceTable.o: ../src/IFaceTable.cxx ../src/IFaceTable.h
JobQueue.o: ../src/JobQueue.cxx ../../scintilla/include/Scintilla.h \
 ../src/GUI.h ../src/SString.h ../src/FilePath.h ../src/SciTE.h \
 ../src/Mutex.h ../src/JobQueue.h
LuaExtension.o: ../src/LuaExtension.cxx \
 ../../scintilla/include/Scintilla.h ../src/GUI.h ../src/SString.h \
 ../src/FilePath.h ../src/StyleWriter.h ../src/Extender.h \
 ../src/LuaExtension.h ../src/IFaceTable.h ../src/SciTEKeys.h \
 ../lua/include/lua.h ../lua/include/luaconf.h ../lua/include/lualib.h \
 ../lua/include/lua.h ../lua/include/lauxlib.h
MultiplexExtension.o: ../src/MultiplexExtension.cxx \
 ../../scintilla/include/Scintilla.h ../src/GUI.h \
 ../src/MultiplexExtension.h ../src/Extender.h
PropSetFile.o: ../src/PropSetFile.cxx ../../scintilla/include/Scintilla.h \
 ../src/GUI.h ../src/SString.h ../src/FilePath.h ../src/PropSetFile.h
SciTEBase.o: ../src/SciTEBase.cxx \
 ../../scintilla/include/Scintilla.h ../../scintilla/include/SciLexer.h \
 ../../scintilla/include/ILexer.h ../src/GUI.h ../src/SString.h \
 ../src/StringList.h ../src/StringHelpers.h ../src/FilePath.h \
 ../src/PropSetFile.h ../src/StyleWriter.h ../src/Extender.h \
 ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h ../src/Cookie.h \
 ../src/Worker.h ../src/FileWorker.h ../src/SciTEBase.h
SciTEBuffers.o: ../src/SciTEBuffers.cxx \
 ../../scintilla/include/Scintilla.h ../../scintilla/include/SciLexer.h \
 ../../scintilla/include/ILexer.h ../src/GUI.h ../src/SString.h \
 ../src/StringList.h ../src/StringHelpers.h ../src/FilePath.h \
 ../src/PropSetFile.h ../src/StyleWriter.h ../src/Extender.h \
 ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h ../src/Cookie.h \
 ../src/Worker.h ../src/FileWorker.h ../src/SciTEBase.h
SciTEIO.o: ../src/SciTEIO.cxx \
 ../../scintilla/include/Scintilla.h ../../scintilla/include/ILexer.h \
 ../src/GUI.h ../src/SString.h ../src/StringList.h ../src/StringHelpers.h \
 ../src/FilePath.h ../src/PropSetFile.h ../src/StyleWriter.h \
 ../src/Extender.h ../src/SciTE.h ../src/Mutex.h ../src/JobQueue.h \
 ../src/Cookie.h ../src/Worker.h ../src/FileWorker.h ../src/SciTEBase.h \
 ../src/Utf8_16.h
SciTEProps.o: ../src/SciTEProps.cxx ../../scintilla/include/Scintilla.h \
 ../../scintilla/include/SciLexer.h ../../scintilla/include/ILexer.h \
 ../src/GUI.h \
 ../src/StringList.h ../src/StringHelpers.h ../src/FilePath.h \
 ../src/PropSetFile.h ../src/StyleWriter.h ../src/Extender.h \
 ../src/SciTE.h ../src/IFaceTable.h ../src/Mutex.h ../src/JobQueue.h \
 ../src/Cookie.h ../src/Worker.h ../src/SciTEBase.h
StringHelpers.o: ../src/StringHelpers.cxx \
 ../../scintilla/include/Scintilla.h ../src/GUI.h ../src/StringHelpers.h
StringList.o: ../src/StringList.cxx ../src/SString.h ../src/StringList.h
StyleWriter.o: ../src/StyleWriter.cxx ../../scintilla/include/Scintilla.h \
 ../src/GUI.h ../src/StyleWriter.h
Utf8_16.o: ../src/Utf8_16.cxx ../src/Utf8_16.h
