#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <string>
namespace {
constexpr wchar_t kClassName[] = L"CaptureFrameNativeWindow";
constexpr UINT WM_TRAY = WM_APP + 1; constexpr UINT ID_TRAY = 1;
constexpr int HOTKEY_TOGGLE=1,HOTKEY_PASS=2,HOTKEY_EXIT=3;
constexpr UINT ID_PRESET_SAVE_BASE=1100, ID_PRESET_LOAD_BASE=1200;
constexpr UINT ID_COLOR_RED=1001,ID_COLOR_BLUE=1002,ID_COLOR_GREEN=1003,ID_STYLE_DASH=1011,ID_STYLE_DOT=1012,ID_STYLE_SOLID=1013,ID_WIDTH_1=1021,ID_WIDTH_2=1022,ID_WIDTH_3=1023,ID_SHOW=1031,ID_PASS=1032,ID_EXIT=1033;
COLORREF g_color=RGB(255,55,55); int g_penStyle=PS_DASH,g_penWidth=2; bool g_visible=true,g_passthrough=false,g_dragging=false,g_resizing=false;
HWND g_hwnd=nullptr; POINT g_dragStart{}; RECT g_startRect{}; int g_edges=0; NOTIFYICONDATAW g_nid{};
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
void AddTray(){g_nid={};g_nid.cbSize=sizeof(g_nid);g_nid.hWnd=g_hwnd;g_nid.uID=ID_TRAY;g_nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;g_nid.uCallbackMessage=WM_TRAY;g_nid.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(g_nid.szTip,L"Capture Frame - OBS 录制区域参考框");Shell_NotifyIconW(NIM_ADD,&g_nid);}
void RemoveTray(){Shell_NotifyIconW(NIM_DELETE,&g_nid);}

void SavePreset(int slot){
 auto p=IniPath();wchar_t section[32],b[64];swprintf_s(section,L"Preset%d",slot);
 RECT r{};GetWindowRect(g_hwnd,&r);
 auto put=[&](const wchar_t*k,int v){swprintf_s(b,L"%d",v);WritePrivateProfileStringW(section,k,b,p.c_str());};
 put(L"X",r.left);put(L"Y",r.top);put(L"W",r.right-r.left);put(L"H",r.bottom-r.top);put(L"Color",(int)g_color);put(L"Style",g_penStyle);put(L"Width",g_penWidth);
}
void LoadPreset(int slot){
 auto p=IniPath();wchar_t section[32];swprintf_s(section,L"Preset%d",slot);
 int x=GetPrivateProfileIntW(section,L"X",-99999,p.c_str());if(x==-99999){MessageBoxW(g_hwnd,L"这个收藏位还没有保存设置。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;}
 int y=GetPrivateProfileIntW(section,L"Y",120,p.c_str()),w=GetPrivateProfileIntW(section,L"W",960,p.c_str()),h=GetPrivateProfileIntW(section,L"H",540,p.c_str());
 g_color=(COLORREF)GetPrivateProfileIntW(section,L"Color",(int)RGB(255,55,55),p.c_str());g_penStyle=GetPrivateProfileIntW(section,L"Style",PS_DASH,p.c_str());g_penWidth=GetPrivateProfileIntW(section,L"Width",2,p.c_str());
 SetWindowPos(g_hwnd,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE|SWP_NOCOPYBITS);InvalidateRect(g_hwnd,nullptr,TRUE);UpdateWindow(g_hwnd);SaveSettings();
}
\nvoid PopupMenu(){POINT pt{};GetCursorPos(&pt);HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING|(g_visible?MF_CHECKED:0),ID_SHOW,L"显示 / 隐藏框   Ctrl+Alt+F");AppendMenuW(m,MF_STRING|(g_passthrough?MF_CHECKED:0),ID_PASS,L"鼠标穿透   Ctrl+Alt+P");AppendMenuW(m,MF_SEPARATOR,0,nullptr);
HMENU c=CreatePopupMenu();AppendMenuW(c,MF_STRING|(g_color==RGB(255,55,55)?MF_CHECKED:0),ID_COLOR_RED,L"红色");AppendMenuW(c,MF_STRING|(g_color==RGB(55,150,255)?MF_CHECKED:0),ID_COLOR_BLUE,L"蓝色");AppendMenuW(c,MF_STRING|(g_color==RGB(70,220,120)?MF_CHECKED:0),ID_COLOR_GREEN,L"绿色");AppendMenuW(m,MF_POPUP,(UINT_PTR)c,L"边框颜色");
HMENU s=CreatePopupMenu();AppendMenuW(s,MF_STRING|(g_penStyle==PS_DASH?MF_CHECKED:0),ID_STYLE_DASH,L"虚线");AppendMenuW(s,MF_STRING|(g_penStyle==PS_DOT?MF_CHECKED:0),ID_STYLE_DOT,L"点线");AppendMenuW(s,MF_STRING|(g_penStyle==PS_SOLID?MF_CHECKED:0),ID_STYLE_SOLID,L"实线");AppendMenuW(m,MF_POPUP,(UINT_PTR)s,L"线条样式");
HMENU w=CreatePopupMenu();AppendMenuW(w,MF_STRING|(g_penWidth==1?MF_CHECKED:0),ID_WIDTH_1,L"细");AppendMenuW(w,MF_STRING|(g_penWidth==2?MF_CHECKED:0),ID_WIDTH_2,L"中");AppendMenuW(w,MF_STRING|(g_penWidth==4?MF_CHECKED:0),ID_WIDTH_3,L"粗");AppendMenuW(m,MF_POPUP,(UINT_PTR)w,L"线条粗细");
HMENU presets=CreatePopupMenu(),saveSlots=CreatePopupMenu(),loadSlots=CreatePopupMenu();
AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+1,L"收藏位 1");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+2,L"收藏位 2");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+3,L"收藏位 3");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+4,L"收藏位 4");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+5,L"收藏位 5");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+6,L"收藏位 6");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+7,L"收藏位 7");AppendMenuW(saveSlots,MF_STRING,ID_PRESET_SAVE_BASE+8,L"收藏位 8");
AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+1,L"收藏位 1");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+2,L"收藏位 2");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+3,L"收藏位 3");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+4,L"收藏位 4");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+5,L"收藏位 5");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+6,L"收藏位 6");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+7,L"收藏位 7");AppendMenuW(loadSlots,MF_STRING,ID_PRESET_LOAD_BASE+8,L"收藏位 8");
AppendMenuW(presets,MF_POPUP,(UINT_PTR)saveSlots,L"保存当前设置到...");AppendMenuW(presets,MF_POPUP,(UINT_PTR)loadSlots,L"切换到...");AppendMenuW(m,MF_POPUP,(UINT_PTR)presets,L"收藏预设");
AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,ID_EXIT,L"退出");SetForegroundWindow(g_hwnd);UINT cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,g_hwnd,nullptr);DestroyMenu(m);
switch(cmd){case ID_SHOW:SetVisible(!g_visible);break;case ID_PASS:g_passthrough=!g_passthrough;ApplyStyle();break;case ID_COLOR_RED:g_color=RGB(255,55,55);break;case ID_COLOR_BLUE:g_color=RGB(55,150,255);break;case ID_COLOR_GREEN:g_color=RGB(70,220,120);break;case ID_STYLE_DASH:g_penStyle=PS_DASH;break;case ID_STYLE_DOT:g_penStyle=PS_DOT;break;case ID_STYLE_SOLID:g_penStyle=PS_SOLID;break;case ID_WIDTH_1:g_penWidth=1;break;case ID_WIDTH_2:g_penWidth=2;break;case ID_WIDTH_3:g_penWidth=4;break;case ID_EXIT:DestroyWindow(g_hwnd);return;}case ID_PRESET_SAVE_BASE+1:SavePreset(1);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+1:LoadPreset(1);return;case ID_PRESET_SAVE_BASE+2:SavePreset(2);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+2:LoadPreset(2);return;case ID_PRESET_SAVE_BASE+3:SavePreset(3);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+3:LoadPreset(3);return;case ID_PRESET_SAVE_BASE+4:SavePreset(4);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+4:LoadPreset(4);return;case ID_PRESET_SAVE_BASE+5:SavePreset(5);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+5:LoadPreset(5);return;case ID_PRESET_SAVE_BASE+6:SavePreset(6);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+6:LoadPreset(6);return;case ID_PRESET_SAVE_BASE+7:SavePreset(7);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+7:LoadPreset(7);return;case ID_PRESET_SAVE_BASE+8:SavePreset(8);MessageBoxW(g_hwnd,L"当前区域设置已收藏。",L"Capture Frame",MB_OK|MB_ICONINFORMATION);return;case ID_PRESET_LOAD_BASE+8:LoadPreset(8);return;case ID_EXIT:DestroyWindow(g_hwnd);return;}SaveSettings();InvalidateRect(g_hwnd,nullptr,TRUE);}
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
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int){int x=ReadInt(L"X",160),y=ReadInt(L"Y",120),w=ReadInt(L"W",960),h=ReadInt(L"H",540);g_color=(COLORREF)ReadInt(L"Color",(int)RGB(255,55,55));g_penStyle=ReadInt(L"Style",PS_DASH);g_penWidth=ReadInt(L"Width",2);WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=WndProc;wc.lpszClassName=kClassName;wc.hCursor=LoadCursorW(nullptr,IDC_CROSS);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);g_hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,kClassName,L"Capture Frame",WS_POPUP,x,y,w,h,nullptr,nullptr,inst,nullptr);if(!g_hwnd)return 1;ApplyStyle();AddTray();RegisterHotKey(g_hwnd,HOTKEY_TOGGLE,MOD_CONTROL|MOD_ALT,'F');RegisterHotKey(g_hwnd,HOTKEY_PASS,MOD_CONTROL|MOD_ALT,'P');RegisterHotKey(g_hwnd,HOTKEY_EXIT,MOD_CONTROL|MOD_ALT,VK_ESCAPE);ShowWindow(g_hwnd,SW_SHOWNOACTIVATE);UpdateWindow(g_hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;}
