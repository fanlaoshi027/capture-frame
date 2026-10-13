#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <string>
#include <algorithm>
namespace {
constexpr wchar_t kClassName[] = L"CaptureFrameNativeWindow";
constexpr UINT WM_TRAY = WM_APP + 1; constexpr UINT ID_TRAY = 1;
constexpr int HOTKEY_TOGGLE=1,HOTKEY_PASS=2,HOTKEY_EXIT=3;
constexpr UINT ID_PRESET_SAVE_BASE=1100, ID_PRESET_LOAD_BASE=1200;
constexpr UINT ID_COLOR_RED=1001,ID_COLOR_BLUE=1002,ID_COLOR_GREEN=1003,ID_STYLE_DASH=1011,ID_STYLE_DOT=1012,ID_STYLE_SOLID=1013,ID_WIDTH_1=1021,ID_WIDTH_2=1022,ID_WIDTH_3=1023,ID_SHOW=1031,ID_PASS=1032,ID_EXIT=1033;
COLORREF g_color=RGB(255,55,55); int g_penStyle=PS_DASH,g_penWidth=2; bool g_visible=true,g_passthrough=false,g_dragging=false,g_resizing=false;
HWND g_hwnd=nullptr; HICON g_appIcon=nullptr; POINT g_dragStart{}; RECT g_startRect{}; int g_edges=0; NOTIFYICONDATAW g_nid{};
enum{EDGE_L=1,EDGE_T=2,EDGE_R=4,EDGE_B=8};
std::wstring IniPath(){wchar_t b[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",b,MAX_PATH);std::wstring p=n&&n<MAX_PATH?b:L".";p+=L"\\CaptureFrame";CreateDirectoryW(p.c_str(),nullptr);p+=L"\\CaptureFrame.ini";return p;}
int ReadInt(const wchar_t*k,int d){auto p=IniPath();return GetPrivateProfileIntW(L"Frame",k,d,p.c_str());}
void SaveSettings(){auto p=IniPath();wchar_t b[64];RECT r{};GetWindowRect(g_hwnd,&r);
#define SAVE(k,v) swprintf_s(b,L"%d",(int)(v));WritePrivateProfileStringW(L"Frame",L##k,b,p.c_str())
SAVE("X",r.left);SAVE("Y",r.top);SAVE("W",r.right-r.left);SAVE("H",r.bottom-r.top);SAVE("Color",g_color);SAVE("Style",g_penStyle);SAVE("Width",g_penWidth);
#undef SAVE
}
void ApplyStyle(){LONG_PTR ex=GetWindowLongPtrW(g_hwnd,GWL_EXSTYLE)|WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW;if(g_passthrough)ex|=WS_EX_TRANSPARENT;else ex&=~WS_EX_TRANSPARENT;SetWindowLongPtrW(g_hwnd,GWL_EXSTYLE,ex);SetLayeredWindowAttributes(g_hwnd,RGB(255,0,255),0,LWA_COLORKEY);SetWindowPos(g_hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED);}
void SetVisible(bool v){g_visible=v;ShowWindow(g_hwnd,v?SW_SHOWNOACTIVATE:SW_HIDE);}
void AddTray(){g_nid={};g_nid.cbSize=sizeof(g_nid);g_nid.hWnd=g_hwnd;g_nid.uID=ID_TRAY;g_nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;g_nid.uCallbackMessage=WM_TRAY;g_nid.hIcon=g_appIcon?g_appIcon:LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(g_nid.szTip,L"Capture Frame - OBS 录制区域参考框");Shell_NotifyIconW(NIM_ADD,&g_nid);}
void RemoveTray(){Shell_NotifyIconW(NIM_DELETE,&g_nid);}


constexpr int ID_NAME_EDIT=2001, ID_NAME_OK=2002, ID_NAME_CANCEL=2003;
HWND g_nameEdit=nullptr; std::wstring g_promptResult; bool g_promptDone=false;
HICON MakeAppIcon(){
 HDC screen=GetDC(nullptr),mem=CreateCompatibleDC(screen);BITMAPV5HEADER bi{};bi.bV5Size=sizeof(bi);bi.bV5Width=64;bi.bV5Height=-64;bi.bV5Planes=1;bi.bV5BitCount=32;bi.bV5Compression=BI_RGB;
 void* bits=nullptr;HBITMAP bmp=CreateDIBSection(screen,(BITMAPINFO*)&bi,DIB_RGB_COLORS,&bits,nullptr,0);HGDIOBJ old=SelectObject(mem,bmp);
 RECT rc{0,0,64,64};HBRUSH bg=CreateSolidBrush(RGB(18,39,68));FillRect(mem,&rc,bg);DeleteObject(bg);
 HPEN pen=CreatePen(PS_SOLID,4,RGB(255,70,70));HGDIOBJ op=SelectObject(mem,pen);HGDIOBJ ob=SelectObject(mem,GetStockObject(NULL_BRUSH));Rectangle(mem,10,14,54,50);
 SelectObject(mem,ob);SelectObject(mem,op);DeleteObject(pen);
 HBITMAP mask=CreateBitmap(64,64,1,1,nullptr);ICONINFO ii{};ii.fIcon=TRUE;ii.hbmColor=bmp;ii.hbmMask=mask;HICON icon=CreateIconIndirect(&ii);
 SelectObject(mem,old);DeleteObject(mask);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(nullptr,screen);return icon;
}
LRESULT CALLBACK NameDialogProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:
  CreateWindowExW(0,L"STATIC",L"给这套录制区域起个名字：",WS_CHILD|WS_VISIBLE,16,14,280,24,h,nullptr,nullptr,nullptr);
  g_nameEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,16,42,300,26,h,(HMENU)ID_NAME_EDIT,nullptr,nullptr);
  CreateWindowExW(0,L"BUTTON",L"保存",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,150,82,78,28,h,(HMENU)ID_NAME_OK,nullptr,nullptr);
  CreateWindowExW(0,L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|WS_TABSTOP,238,82,78,28,h,(HMENU)ID_NAME_CANCEL,nullptr,nullptr);
  SetFocus(g_nameEdit);return 0;
 case WM_COMMAND:
  if(LOWORD(wp)==ID_NAME_OK){wchar_t text[128]{};GetWindowTextW(g_nameEdit,text,128);if(!text[0]){MessageBoxW(h,L"请输入一个容易辨认的名称。",L"收藏预设",MB_OK|MB_ICONINFORMATION);return 0;}g_promptResult=text;g_promptDone=true;DestroyWindow(h);return 0;}
  if(LOWORD(wp)==ID_NAME_CANCEL){g_promptResult.clear();g_promptDone=true;DestroyWindow(h);return 0;}break;
 case WM_CLOSE:g_promptResult.clear();g_promptDone=true;DestroyWindow(h);return 0;
 }
 return DefWindowProcW(h,msg,wp,lp);
}
std::wstring AskPresetName(int slot){
 wchar_t section[32],existing[128]{};swprintf_s(section,L"Preset%d",slot);auto p=IniPath();GetPrivateProfileStringW(section,L"Name",L"",existing,128,p.c_str());
 g_promptResult.clear();g_promptDone=false;HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST,L"CaptureFrameNameDialog",L"保存收藏预设",WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,350,155,g_hwnd,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!dlg)return L"";EnableWindow(g_hwnd,FALSE);if(existing[0])SetWindowTextW(g_nameEdit,existing);else{wchar_t def[32];swprintf_s(def,L"预设 %d",slot);SetWindowTextW(g_nameEdit,def);}SetWindowPos(dlg,nullptr,(GetSystemMetrics(SM_CXSCREEN)-350)/2,(GetSystemMetrics(SM_CYSCREEN)-155)/2,350,155,SWP_NOZORDER|SWP_SHOWWINDOW);SetForegroundWindow(dlg);SetFocus(g_nameEdit);
 MSG msg;while(!g_promptDone&&GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(dlg,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 EnableWindow(g_hwnd,TRUE);SetForegroundWindow(g_hwnd);return g_promptResult;
}

void SavePreset(int slot,const std::wstring& name){
 auto p=IniPath();wchar_t section[32],b[64];swprintf_s(section,L"Preset%d",slot);
 RECT r{};GetWindowRect(g_hwnd,&r);
 auto put=[&](const wchar_t*k,int v){swprintf_s(b,L"%d",v);WritePrivateProfileStringW(section,k,b,p.c_str());};
 put(L"X",r.left);put(L"Y",r.top);put(L"W",r.right-r.left);put(L"H",r.bottom-r.top);put(L"Color",(int)g_color);put(L"Style",g_penStyle);put(L"Width",g_penWidth);WritePrivateProfileStringW(section,L"Name",name.c_str(),p.c_str());
}
void LoadPreset(int slot){
 auto p=IniPath();wchar_t section[32];swprintf_s(section,L"Preset%d",slot);
 int x=GetPrivateProfileIntW(section,L"X",-99999,p.c_str());if(x==-99999){MessageBoxW(g_hwnd,L"这个收藏位还没有保存设置。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;}
 int y=GetPrivateProfileIntW(section,L"Y",120,p.c_str()),w=GetPrivateProfileIntW(section,L"W",960,p.c_str()),h=GetPrivateProfileIntW(section,L"H",540,p.c_str());
 g_color=(COLORREF)GetPrivateProfileIntW(section,L"Color",(int)RGB(255,55,55),p.c_str());g_penStyle=GetPrivateProfileIntW(section,L"Style",PS_DASH,p.c_str());g_penWidth=GetPrivateProfileIntW(section,L"Width",2,p.c_str());
 SetWindowPos(g_hwnd,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE|SWP_NOCOPYBITS);InvalidateRect(g_hwnd,nullptr,TRUE);UpdateWindow(g_hwnd);SaveSettings();
}
void PopupMenu(){POINT pt{};GetCursorPos(&pt);HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING|(g_visible?MF_CHECKED:0),ID_SHOW,L"显示 / 隐藏框   Ctrl+Alt+F");AppendMenuW(m,MF_STRING|(g_passthrough?MF_CHECKED:0),ID_PASS,L"鼠标穿透   Ctrl+Alt+P");AppendMenuW(m,MF_SEPARATOR,0,nullptr);
HMENU c=CreatePopupMenu();AppendMenuW(c,MF_STRING|(g_color==RGB(255,55,55)?MF_CHECKED:0),ID_COLOR_RED,L"红色");AppendMenuW(c,MF_STRING|(g_color==RGB(55,150,255)?MF_CHECKED:0),ID_COLOR_BLUE,L"蓝色");AppendMenuW(c,MF_STRING|(g_color==RGB(70,220,120)?MF_CHECKED:0),ID_COLOR_GREEN,L"绿色");AppendMenuW(m,MF_POPUP,(UINT_PTR)c,L"边框颜色");
HMENU s=CreatePopupMenu();AppendMenuW(s,MF_STRING|(g_penStyle==PS_DASH?MF_CHECKED:0),ID_STYLE_DASH,L"虚线");AppendMenuW(s,MF_STRING|(g_penStyle==PS_DOT?MF_CHECKED:0),ID_STYLE_DOT,L"点线");AppendMenuW(s,MF_STRING|(g_penStyle==PS_SOLID?MF_CHECKED:0),ID_STYLE_SOLID,L"实线");AppendMenuW(m,MF_POPUP,(UINT_PTR)s,L"线条样式");
HMENU w=CreatePopupMenu();AppendMenuW(w,MF_STRING|(g_penWidth==1?MF_CHECKED:0),ID_WIDTH_1,L"细");AppendMenuW(w,MF_STRING|(g_penWidth==2?MF_CHECKED:0),ID_WIDTH_2,L"中");AppendMenuW(w,MF_STRING|(g_penWidth==4?MF_CHECKED:0),ID_WIDTH_3,L"粗");AppendMenuW(m,MF_POPUP,(UINT_PTR)w,L"线条粗细");
HMENU presets=CreatePopupMenu(),saveSlots=CreatePopupMenu(),loadSlots=CreatePopupMenu();
for(int i=1;i<=8;i++){wchar_t label[160],section[32],name[128]{};swprintf_s(section,L"Preset%d",i);GetPrivateProfileStringW(section,L"Name",L"",name,128,IniPath().c_str());if(name[0])swprintf_s(label,L"%d. %s",i,name);else swprintf_s(label,L"%d. (未命名)",i);AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+i,label);}
for(int i=1;i<=8;i++){wchar_t label[160],section[32],name[128]{};swprintf_s(section,L"Preset%d",i);GetPrivateProfileStringW(section,L"Name",L"",name,128,IniPath().c_str());if(name[0])swprintf_s(label,L"%d. %s",i,name);else swprintf_s(label,L"%d. (未命名)",i);AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+i,label);}
AppendMenuW(presets,MF_POPUP,(UINT_PTR)saveSlots,L"保存当前设置到...");AppendMenuW(presets,MF_POPUP,(UINT_PTR)loadSlots,L"切换到...");AppendMenuW(m,MF_POPUP,(UINT_PTR)presets,L"收藏预设");
AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,ID_EXIT,L"退出");SetForegroundWindow(g_hwnd);UINT cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,g_hwnd,nullptr);DestroyMenu(m);
 switch(cmd){case ID_SHOW:SetVisible(!g_visible);break;case ID_PASS:g_passthrough=!g_passthrough;ApplyStyle();break;case ID_COLOR_RED:g_color=RGB(255,55,55);break;case ID_COLOR_BLUE:g_color=RGB(55,150,255);break;case ID_COLOR_GREEN:g_color=RGB(70,220,120);break;case ID_STYLE_DASH:g_penStyle=PS_DASH;break;case ID_STYLE_DOT:g_penStyle=PS_DOT;break;case ID_STYLE_SOLID:g_penStyle=PS_SOLID;break;case ID_WIDTH_1:g_penWidth=1;break;case ID_WIDTH_2:g_penWidth=2;break;case ID_WIDTH_3:g_penWidth=4;break;case ID_PRESET_SAVE_BASE+1:{auto name=AskPresetName(1);if(!name.empty()){SavePreset(1,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+1:LoadPreset(1);return;case ID_PRESET_SAVE_BASE+2:{auto name=AskPresetName(2);if(!name.empty()){SavePreset(2,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+2:LoadPreset(2);return;case ID_PRESET_SAVE_BASE+3:{auto name=AskPresetName(3);if(!name.empty()){SavePreset(3,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+3:LoadPreset(3);return;case ID_PRESET_SAVE_BASE+4:{auto name=AskPresetName(4);if(!name.empty()){SavePreset(4,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+4:LoadPreset(4);return;case ID_PRESET_SAVE_BASE+5:{auto name=AskPresetName(5);if(!name.empty()){SavePreset(5,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+5:LoadPreset(5);return;case ID_PRESET_SAVE_BASE+6:{auto name=AskPresetName(6);if(!name.empty()){SavePreset(6,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+6:LoadPreset(6);return;case ID_PRESET_SAVE_BASE+7:{auto name=AskPresetName(7);if(!name.empty()){SavePreset(7,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+7:LoadPreset(7);return;case ID_PRESET_SAVE_BASE+8:{auto name=AskPresetName(8);if(!name.empty()){SavePreset(8,name);MessageBoxW(g_hwnd,L"收藏已保存。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);}return;}case ID_PRESET_LOAD_BASE+8:LoadPreset(8);return;case ID_EXIT:DestroyWindow(g_hwnd);return;}SaveSettings();InvalidateRect(g_hwnd,nullptr,TRUE);}
int HitEdges(POINT p,RECT r){int e=0;const int m=12;if(p.x-r.left<m)e|=EDGE_L;if(r.right-p.x<m)e|=EDGE_R;if(p.y-r.top<m)e|=EDGE_T;if(r.bottom-p.y<m)e|=EDGE_B;return e;}
LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
case WM_ERASEBKGND:return 1;
case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);HBRUSH bg=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);HPEN pen=CreatePen(g_penStyle,g_penWidth,g_color);HGDIOBJ op=SelectObject(dc,pen),ob=SelectObject(dc,GetStockObject(NULL_BRUSH));InflateRect(&r,g_penWidth,g_penWidth);Rectangle(dc,r.left+g_penWidth,r.top+g_penWidth,r.right-g_penWidth,r.bottom-g_penWidth);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(pen);EndPaint(h,&ps);return 0;}
case WM_LBUTTONDOWN:{if(g_passthrough)return 0;POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};RECT r{};GetWindowRect(h,&r);g_edges=HitEdges(POINT{p.x+r.left,p.y+r.top},r);g_resizing=g_edges!=0&&!(GetKeyState(VK_SHIFT)&0x8000);g_dragging=!g_resizing;g_dragStart={p.x+r.left,p.y+r.top};g_startRect=r;SetCapture(h);return 0;}
case WM_MOUSEMOVE:{if(g_passthrough)return 0;if(g_dragging&&(wp&MK_LBUTTON)){POINT p{};GetCursorPos(&p);SetWindowPos(h,HWND_TOPMOST,g_startRect.left+p.x-g_dragStart.x,g_startRect.top+p.y-g_dragStart.y,g_startRect.right-g_startRect.left,g_startRect.bottom-g_startRect.top,SWP_NOACTIVATE|SWP_NOCOPYBITS);InvalidateRect(h,nullptr,TRUE);UpdateWindow(h);}else if(g_resizing&&(wp&MK_LBUTTON)){POINT p{};GetCursorPos(&p);RECT r=g_startRect;if(g_edges&EDGE_L)r.left=min(p.x,g_startRect.right-120);if(g_edges&EDGE_R)r.right=max(p.x,g_startRect.left+120);if(g_edges&EDGE_T)r.top=min(p.y,g_startRect.bottom-80);if(g_edges&EDGE_B)r.bottom=max(p.y,g_startRect.top+80);SetWindowPos(h,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_NOCOPYBITS);InvalidateRect(h,nullptr,TRUE);UpdateWindow(h);}return 0;}
case WM_LBUTTONUP:if(g_dragging||g_resizing){g_dragging=g_resizing=false;ReleaseCapture();SaveSettings();}return 0;
case WM_HOTKEY:if(wp==HOTKEY_TOGGLE)SetVisible(!g_visible);else if(wp==HOTKEY_PASS){g_passthrough=!g_passthrough;ApplyStyle();}else if(wp==HOTKEY_EXIT)DestroyWindow(h);return 0;
case WM_TRAY:if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)PopupMenu();else if(lp==WM_LBUTTONDBLCLK)SetVisible(!g_visible);return 0;
case WM_DESTROY:SaveSettings();RemoveTray();UnregisterHotKey(h,HOTKEY_TOGGLE);UnregisterHotKey(h,HOTKEY_PASS);UnregisterHotKey(h,HOTKEY_EXIT);PostQuitMessage(0);return 0;}return DefWindowProcW(h,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int){int x=ReadInt(L"X",160),y=ReadInt(L"Y",120),w=ReadInt(L"W",960),h=ReadInt(L"H",540);g_color=(COLORREF)ReadInt(L"Color",(int)RGB(255,55,55));g_penStyle=ReadInt(L"Style",PS_DASH);g_penWidth=ReadInt(L"Width",2);WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=WndProc;wc.lpszClassName=kClassName;wc.hCursor=LoadCursorW(nullptr,IDC_CROSS);g_appIcon=MakeAppIcon();wc.hIcon=g_appIcon;RegisterClassW(&wc);WNDCLASSW nw{};nw.hInstance=inst;nw.lpfnWndProc=NameDialogProc;nw.lpszClassName=L"CaptureFrameNameDialog";nw.hCursor=LoadCursorW(nullptr,IDC_ARROW);nw.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&nw);g_hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,kClassName,L"Capture Frame",WS_POPUP,x,y,w,h,nullptr,nullptr,inst,nullptr);if(!g_hwnd){DWORD e=GetLastError();wchar_t msg[256];swprintf_s(msg,L"程序窗口创建失败（错误代码 %lu）。\n此版本为原生 Windows 程序，一般不需要额外安装 .NET 或 VC++ 运行库。请确认 Windows 系统组件正常。",e);MessageBoxW(nullptr,msg,L"Capture Frame 启动失败",MB_OK|MB_ICONERROR);return 1;}ApplyStyle();AddTray();RegisterHotKey(g_hwnd,HOTKEY_TOGGLE,MOD_CONTROL|MOD_ALT,'F');RegisterHotKey(g_hwnd,HOTKEY_PASS,MOD_CONTROL|MOD_ALT,'P');RegisterHotKey(g_hwnd,HOTKEY_EXIT,MOD_CONTROL|MOD_ALT,VK_ESCAPE);ShowWindow(g_hwnd,SW_SHOWNOACTIVATE);UpdateWindow(g_hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;}
