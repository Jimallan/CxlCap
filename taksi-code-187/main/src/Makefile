; TODO - this is out of date! update it

INC=\DXSDK9\Include
CC=cl
CFLAGS=/nologo $(EXTRA_CFLAGS) -GX
LINK=link
LFLAGS=/NOLOGO /LIBPATH:\DXSDK9\Lib
LIBS=kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib vfw32.lib dxguid.lib winmm.lib dinput8.lib comdlg32.lib oleaut32.lib uuid.lib comctl32.lib

all: taksidll.dll taksi.exe
release:
	$(MAKE) EXTRA_CFLAGS=/DNDEBUG
debug:
	$(MAKE) EXTRA_CFLAGS=/D_DEBUG

taksi.exe: taksi.obj guiconfig.obj gui.obj det_dx8.obj det_dx9.obj taksi.res taksi.dll
	$(LINK) $(LFLAGS) /out:taksi.exe taksi.obj gui.obj guiconfig.obj test_dx8.obj test_dx9.obj taksi.lib taksi.res $(LIBS)
	
taksidll.dll: taksidll.lib taksidll.res
	$(LINK) $(LFLAGS) /out:taksi.dll /DLL taksidll.obj avi.obj config.obj graphx.obj graphx_dx8.obj graphx_dx9.obj graphx_gdi.obj graphx_ogl.obj hotkeys.obj taksidll.res $(LIBS)
	
taksi.obj: exe/taksi.cpp exe/guiConfig.h exe/taksi.h stdafx.h
test_dx8.obj: exe/test_dx8.cpp exe/taksi.h stdafx.h
test_dx9.obj: exe/test_dx9.cpp exe/taksi.h  stdafx.h
gui.obj: exe/gui.cpp exe/guiconfig.h exe/taksi.h stdafx.h
guiconfig.obj: exe/guiconfig.cpp exe/guiconfig.h exe/taksi.h stdafx.h

Taksi.res: 
	rc -r -fo Taksi.res exe/Taksi.rc

taksidll.obj: dll/taksidll.cpp dll/taksidll.h stdafx.h
avi.obj: dll/avi.cpp dll/graphx.h dll/hotkeys.h dll/taksidll.h stdafx.h
config.obj: dll/config.cpp dll/taksidll.h stdafx.h
graphx.obj: dll/graphx.cpp dll/graphx.h dll/taksidll.h stdafx.h 
graphx_dx8.obj: dll/graphx_dx8.cpp dll/graphx.h dll/taksidll.h stdafx.h  
graphx_dx9.obj: dll/graphx_dx9.cpp dll/graphx.h dll/taksidll.h stdafx.h  
graphx_gdi.obj: dll/graphx_gdi.cpp dll/graphx.h dll/taksidll.h stdafx.h 
graphx_ogl.obj: dll/graphx_ogl.cpp dll/graphx.h dll/taksidll.h stdafx.h 
hotkeys.obj: dll/hotkeys.cpp dll/taksidll.h stdafx.h

TaksiDll.res: 
	rc -r -fo TaksiDll.res dll/TaksiDll.rc

taksidll.lib: taksidll.obj avi.obj config.obj graphx.obj graphx_dx8.obj graphx_dx9.obj graphx_gdi.obj graphx_ogl.obj hotkeys.obj TaksiDll.res

.cpp.obj:
	$(CC) $(CFLAGS) -c /I $(INC) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.log *~
