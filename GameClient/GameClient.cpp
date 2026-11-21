// GameClient.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "GameClient.h"

#include "PlayerViewport.h"
#include "ServerBridge.h"
#include "StartMenu.h"
#include "WaitingRoom.h"
#include "PuzzleArena.h"
#include "RoundSummary.h"
#include "GridPainter.h"

#include <string>
#include <sstream>
#include <mmsystem.h>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdiplus.lib")

#include <array>
#include "../Shared/PacketKinds.h"

#define MAX_LOADSTRING 100

#define IDC_NAME_EDIT         1001
#define IDC_ROOM_EDIT         1002
#define IDC_PLAYERCOUNT_COMBO 1003
#define IDC_PASSWORD_EDIT     1005
#define IDC_VISIBILITY_COMBO  1006

#define TIMER_ID_MAIN        1
#define TIMER_INTERVAL_MS    16

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

struct UiButton
{
    RECT rect{};
    std::wstring text;
    bool enabled = true;
    bool completed = false;
};

struct AppState
{
    ServerBridge bridge;
    PlayerViewport viewport;
    StartMenu startMenu;
    WaitingRoom waitingRoom;
    PuzzleArena puzzleArena;
    RoundSummary roundSummary;
    GridPainter gridPainter;

    bool connected = false;
    bool pendingRoomRequest = false;
    bool authenticated = false;

    std::string statusText = "Starting client...";
    std::string lastRoomText;

    HWND hNameEdit = nullptr;
    HWND hRoomEdit = nullptr;
    HWND hPlayerCountCombo = nullptr;
    HWND hPasswordEdit = nullptr;
    HWND hVisibilityCombo = nullptr;

    HICON hWinCrownIcon = nullptr;
    HICON hLoseCrownIcon = nullptr;

    UiButton signUpButton;
    UiButton signInButton;
    UiButton createButton;
    UiButton joinButton;
    UiButton historyButton;
    UiButton backButton;
    UiButton readyButton;
    UiButton quitButton;
    UiButton notesButton;
    UiButton hintButton;
    std::array<UiButton, 9> digitButtons;

    std::vector<MatchHistoryRow> historyRows;

    std::vector<PublicRoomInfo> publicRooms;
    std::vector<RECT> publicRoomJoinRects;

    std::vector<ChatMessage> roomChatMessages;

    HWND hChatEdit = nullptr;
    UiButton sendChatButton;

    bool socialPopupOpen = false;
    bool socialChatViewOpen = false;


    UiButton socialLauncherButton;
    UiButton socialSearchButton;
    UiButton socialAddFriendButton;
    UiButton socialAcceptFriendButton;
    UiButton socialPrivateSendButton;
    UiButton socialBackButton;

    HWND hSocialSearchEdit = nullptr;
    HWND hSocialPrivateChatEdit = nullptr;

    std::vector<std::string> friendsList;
    std::vector<std::string> pendingFriendRequests;
    std::vector<PrivateChatMessage> privateChatMessages;

    std::string searchedUsername;
    std::string socialStatusText;
    std::vector<RECT> pendingRequestRects;
    std::vector<RECT> friendRects;

    std::string selectedFriendName;

    UiButton stayHereButton;
    UiButton roundSummaryHomeButton;

    UiButton resumeMatchButton;

    ULONGLONG lastInputTick = 0;
    ULONGLONG lastHeartbeatTick = 0;

    bool presencePopupVisible = false;
    bool localTimedOut = false;

};

static AppState gApp;

static ULONG_PTR g_gdiplusToken = 0;
static Gdiplus::Bitmap* g_startMenuBackground = nullptr;

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

static void InitializeUiLayout(AppState& app);
static void CreateChildControls(HWND hWnd);
static void UpdateControlVisibility(AppState& app);
static void ProcessIncomingMessages(AppState& app);
static void DrawCurrentScreen(HDC hdc, const RECT& clientRect, AppState& app);
static void DrawStartMenu(HDC hdc, const RECT& clientRect, AppState& app);
static void DrawWaitingRoom(HDC hdc, const RECT& clientRect, AppState& app);
static void DrawPuzzleArena(HDC hdc, const RECT& clientRect, AppState& app);
static int GetHighlightedDigit(const PaintedBoard& painted);
static void DrawRoundSummary(HDC hdc, const RECT& clientRect, AppState& app);
static void DrawHistoryScreen(HDC hdc, const RECT& clientRect, AppState& app);
static void DrawPublicRoomList(HDC hdc, const RECT& clientRect, AppState& app, bool forestChrome);
static RECT GetHistoryBackButtonRect(const RECT& clientRect);
static RECT GetHistoryContentRect(const RECT& clientRect);
static RECT GetHistoryHeaderRect(const RECT& contentRect, int index, int totalColumns);
static RECT GetHistoryCellRect(const RECT& contentRect, int rowTop, int rowHeight, int index, int totalColumns);
static void HandlePuzzleClick(int x, int y, AppState& app, const RECT& clientRect);
static void HandlePuzzleKey(WPARAM wParam, AppState& app);
static bool GetSelectedCell(const LocalBoardState& board, int& outRow, int& outCol);
static RECT GetResponsiveBoardRect(const RECT& clientRect);
static RECT GetSidebarRect(const RECT& clientRect);
static RECT GetNotesButtonRect(const RECT& clientRect);
static RECT GetHintButtonRect(const RECT& clientRect);
static void DrawButton(HDC hdc, const UiButton& button, bool forestChrome = false);
static bool PointInRectEx(const RECT& rect, int x, int y);
static bool RectsIntersect(const RECT& a, const RECT& b);
static void ShowControlIfNotCovered(HWND hWnd, bool baseVisible, const RECT& popupRect);
static std::string ReadWindowTextA(HWND hWnd);
static int ReadComboSelectedInt(HWND hCombo, int fallbackValue);
static int ReadEditInt(HWND hEdit, int fallbackValue);
static std::wstring ToWide(const std::string& text);
static void HandleStartMenuClick(HWND hWnd, int x, int y, AppState& app);
static void HandleWaitingRoomClick(int x, int y, AppState& app);
static RECT MakeRect(int left, int top, int width, int height);
static void UpdateStartMenuLayout(HWND hWnd, AppState& app);
static RECT GetChatPanelRect(const RECT& clientRect);
static RECT GetChatInputRect(const RECT& clientRect);
static RECT GetChatSendButtonRect(const RECT& clientRect);
static void DrawRoomChat(HDC hdc, const RECT& clientRect, AppState& app);
static RECT GetSocialLauncherRect(const RECT& clientRect);
static RECT GetSocialPopupRect(const RECT& clientRect);
static RECT GetSocialSearchRect(const RECT& popupRect);
static RECT GetSocialSearchButtonRect(const RECT& popupRect);
static RECT GetSocialAddFriendButtonRect(const RECT& popupRect);
static RECT GetSocialPrivateInputRect(const RECT& popupRect);
static RECT GetSocialPrivateSendRect(const RECT& popupRect);
static void DrawSocialPopup(HDC hdc, const RECT& clientRect, AppState& app, bool forestChrome);
static void DrawSocialHome(HDC hdc, const RECT& popupRect, AppState& app, bool forestChrome);
static void DrawSocialChatView(HDC hdc, const RECT& popupRect, AppState& app, bool forestChrome);
static RECT GetSocialBackButtonRect(const RECT& popupRect);
static RECT GetSocialChatMessagesRect(const RECT& popupRect);
static bool HandleSocialPopupClick(int x, int y, AppState& app, const RECT& clientRect);
static void StartBackgroundMusic();
static void StopBackgroundMusic();
static void InitGdiplusIfNeeded();
static void ShutdownGdiplus();
static void EnsureStartMenuBackgroundLoaded();
static void ReleaseStartMenuBackground();
static void DrawStartMenuBackground(HDC hdc, const RECT& clientRect);
static void MarkUserActivity(AppState& app);
static void UpdateIdleState(HWND hWnd, AppState& app);
static RECT GetPresencePopupRect(const RECT& clientRect);
static RECT GetPresenceYesButtonRect(const RECT& popupRect);
static RECT GetPresenceResumeButtonRect(const RECT& popupRect);
static void DrawPresencePopup(HDC hdc, const RECT& clientRect, AppState& app);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GAMECLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GAMECLIENT));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GAMECLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        0,
        900,
        760,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    InitializeUiLayout(gApp);
    gApp.viewport.SetPhase(ScreenPhase::StartMenu);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

static RECT MakeRect(int left, int top, int width, int height)
{
    RECT r{ left, top, left + width, top + height };
    return r;
}

static void StartBackgroundMusic()
{
    constexpr int kMusicVolume = 250; // 0 - 1000 (lower = quieter)
    const wchar_t* kMusicAlias = L"bgm";
    const wchar_t* kMusicPath = L"..\\asset\\com net.mp3";

    std::wstring openCmd = L"open \"";
    openCmd += kMusicPath;
    openCmd += L"\" type mpegvideo alias ";
    openCmd += kMusicAlias;
    mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr);

    std::wstring volumeCmd = L"setaudio ";
    volumeCmd += kMusicAlias;
    volumeCmd += L" volume to ";
    volumeCmd += std::to_wstring(kMusicVolume);
    mciSendStringW(volumeCmd.c_str(), nullptr, 0, nullptr);

    std::wstring playCmd = L"play ";
    playCmd += kMusicAlias;
    playCmd += L" repeat";
    mciSendStringW(playCmd.c_str(), nullptr, 0, nullptr);
}

static void StopBackgroundMusic()
{
    const wchar_t* kMusicAlias = L"bgm";
    std::wstring stopCmd = L"stop ";
    stopCmd += kMusicAlias;
    mciSendStringW(stopCmd.c_str(), nullptr, 0, nullptr);

    std::wstring closeCmd = L"close ";
    closeCmd += kMusicAlias;
    mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
}

static void InitGdiplusIfNeeded()
{
    if (g_gdiplusToken != 0)
    {
        return;
    }

    Gdiplus::GdiplusStartupInput input{};
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr);
}

static void ReleaseStartMenuBackground()
{
    delete g_startMenuBackground;
    g_startMenuBackground = nullptr;
}

static void ShutdownGdiplus()
{
    ReleaseStartMenuBackground();

    if (g_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static void EnsureStartMenuBackgroundLoaded()
{
    if (g_startMenuBackground != nullptr)
    {
        return;
    }

    const wchar_t* fileNames[] =
    {
        L"forest_menu_background.png",
        L"sudoku_menu_background.png",
    };

    for (const wchar_t* fileName : fileNames)
    {
        std::wstring paths[4];
        int pathCount = 0;

        paths[pathCount++] = std::wstring(L"..\\asset\\") + fileName;
        paths[pathCount++] = std::wstring(L"..\\..\\asset\\") + fileName;

        wchar_t module[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, module, MAX_PATH) != 0)
        {
            wchar_t* slash = wcsrchr(module, L'\\');
            if (slash != nullptr)
            {
                *(slash + 1) = L'\0';
                std::wstring base(module);
                paths[pathCount++] = base + L"..\\..\\asset\\" + fileName;
                paths[pathCount++] = base + L"..\\asset\\" + fileName;
            }
        }

        for (int i = 0; i < pathCount; ++i)
        {
            Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromFile(paths[i].c_str());
            if (bmp != nullptr && bmp->GetLastStatus() == Gdiplus::Ok)
            {
                g_startMenuBackground = bmp;
                return;
            }

            delete bmp;
        }
    }
}

static void DrawStartMenuBackground(HDC hdc, const RECT& clientRect)
{
    EnsureStartMenuBackgroundLoaded();

    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    if (g_startMenuBackground != nullptr && g_startMenuBackground->GetLastStatus() == Gdiplus::Ok)
    {
        Gdiplus::Graphics g(hdc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        Gdiplus::Rect dest(
            clientRect.left,
            clientRect.top,
            w,
            h);
        g.DrawImage(g_startMenuBackground, dest);
    }
    else
    {
        HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &clientRect, bgBrush);
        DeleteObject(bgBrush);
    }
}

static void UpdateStartMenuLayout(HWND hWnd, AppState& app)
{
    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int margin = max(20, clientW / 40);
    int gap = max(16, clientW / 50);
    int verticalGap = max(8, clientH / 60);

    int titleTop = max(30, clientH / 18);
    int titleHeight = max(48, clientH / 12);
    int contentTop = titleTop + titleHeight + max(30, clientH / 30);

    int leftCardW = (clientW - margin * 2 - gap) / 3;
    if (leftCardW < 280) leftCardW = 280;

    int rightAreaW = clientW - margin * 2 - gap - leftCardW;
    if (rightAreaW < 320) rightAreaW = 320;

    int leftX = margin;
    int rightX = leftX + leftCardW + gap;

    int topCardH = max(210, clientH / 4);
    int bottomCardH = max(170, clientH / 5);
    int leftCardH = topCardH + verticalGap + bottomCardH;

    RECT leftCard = MakeRect(leftX, contentTop, leftCardW, leftCardH);
    RECT topCard = MakeRect(rightX, contentTop, rightAreaW, topCardH);
    RECT bottomCard = MakeRect(rightX, contentTop + topCardH + verticalGap, rightAreaW, bottomCardH);

    int pad = max(18, clientW / 60);
    int controlH = max(28, clientH / 24);
    int buttonH = max(36, clientH / 18);

    // left login area
    int inputW = leftCardW - pad * 2;
    int userEditY = leftCard.top + 98;
    int passEditY = leftCard.top + 184;
    int authButtonY = leftCard.top + 250;

    int authGap = 12;
    int authButtonW = (inputW - authGap) / 2;

    MoveWindow(app.hNameEdit, leftCard.left + pad, userEditY, inputW, controlH, FALSE);
    MoveWindow(app.hPasswordEdit, leftCard.left + pad, passEditY, inputW, controlH, FALSE);

    app.signUpButton.rect = MakeRect(leftCard.left + pad, authButtonY, authButtonW, buttonH);
    app.signInButton.rect = MakeRect(leftCard.left + pad + authButtonW + authGap, authButtonY, authButtonW, buttonH);

    // create game area
    int createPad = pad + 8;
    int comboW = min(180, rightAreaW / 3);
    int createButtonW = min(280, rightAreaW - createPad * 2);

    int comboY = topCard.top + 95;
    int createButtonY = topCard.top + 160;

    MoveWindow(app.hPlayerCountCombo, topCard.left + createPad, comboY, comboW, 220, FALSE);

    int visibilityY = topCard.top + 129;
    MoveWindow(app.hVisibilityCombo, topCard.left + createPad, visibilityY, comboW, 220, FALSE);

    app.createButton.rect = MakeRect(topCard.left + createPad, createButtonY, createButtonW, buttonH);

    // join game area
    int roomEditY = bottomCard.top + 86;
    int joinButtonY = bottomCard.top + 122;

    //int roomEditW = bottomCard.right - bottomCard.left - createPad * 2;
    int roomEditW = comboW;
    int joinButtonW = min(280, rightAreaW - createPad * 2);

    MoveWindow(app.hRoomEdit, bottomCard.left + createPad, roomEditY, roomEditW, controlH, FALSE);
    app.joinButton.rect = MakeRect(bottomCard.left + createPad, joinButtonY, joinButtonW, buttonH);
}

static void InitializeUiLayout(AppState& app)
{
    // Left login card buttons
    app.signUpButton.rect = { 70, 455, 210, 505 };
    app.signUpButton.text = L"Sign Up";

    app.signInButton.rect = { 230, 455, 370, 505 };
    app.signInButton.text = L"Log In";

    // Right top card button
    app.createButton.rect = { 570, 365, 860, 415 };
    app.createButton.text = L"Create Game";

    // Right bottom card button
    app.joinButton.rect = { 570, 585, 860, 635 };
    app.joinButton.text = L"Join Game";

    app.historyButton.rect = { 0, 0, 0, 0 };
    app.historyButton.text = L"History";

    app.backButton.rect = { 0, 0, 0, 0 };
    app.backButton.text = L"Back";

    // Waiting room / puzzle buttons stay separate
    app.readyButton.rect = { 40, 180, 190, 215 };
    app.readyButton.text = L"Ready";

    app.quitButton.rect = { 210, 180, 360, 215 };
    app.quitButton.text = L"Quit";

    app.notesButton.rect = { 640, 80, 760, 115 };
    app.notesButton.text = L"Pencil: OFF";

    app.hintButton.rect = { 780, 80, 880, 115 };
    app.hintButton.text = L"Hint";

    app.sendChatButton.text = L"Send";
    app.socialSearchButton.text = L"Search";
    app.socialAddFriendButton.text = L"Add Friend";
    app.socialAcceptFriendButton.text = L"Accept";
    app.socialPrivateSendButton.text = L"Send";
    app.socialBackButton.text = L"Back";
    app.stayHereButton.text = L"Yes, Continue";
    app.roundSummaryHomeButton.text = L"Home";
    app.resumeMatchButton.text = L"Resume";

    for (int i = 0; i < 9; ++i)
    {
        wchar_t txt[2]{};
        txt[0] = L'1' + i;
        txt[1] = 0;
        app.digitButtons[i].text = txt;
    }
}

static void CreateChildControls(HWND hWnd)
{
    // Left panel: username
    gApp.hNameEdit = CreateWindowExW(
        0, L"EDIT", L"Player",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_CLIPSIBLINGS,
        70, 315, 300, 40,
        hWnd, (HMENU)IDC_NAME_EDIT, hInst, nullptr);

    // Left panel: password
    gApp.hPasswordEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
        70, 405, 300, 40,
        hWnd, (HMENU)IDC_PASSWORD_EDIT, hInst, nullptr);

    // Right top panel: player count
    gApp.hPlayerCountCombo = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        625, 310, 150, 220,
        hWnd, (HMENU)IDC_PLAYERCOUNT_COMBO, hInst, nullptr);

    SendMessageW(gApp.hPlayerCountCombo, CB_ADDSTRING, 0, (LPARAM)L"2");
    SendMessageW(gApp.hPlayerCountCombo, CB_ADDSTRING, 0, (LPARAM)L"3");
    SendMessageW(gApp.hPlayerCountCombo, CB_SETCURSEL, 0, 0);

    // Right bottom panel: join room id
    gApp.hRoomEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        570, 530, 420, 40,
        hWnd, (HMENU)IDC_ROOM_EDIT, hInst, nullptr);
    gApp.hVisibilityCombo = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        625, 360, 160, 220,
        hWnd, (HMENU)IDC_VISIBILITY_COMBO, hInst, nullptr);

    SendMessageW(gApp.hVisibilityCombo, CB_ADDSTRING, 0, (LPARAM)L"Private");
    SendMessageW(gApp.hVisibilityCombo, CB_ADDSTRING, 0, (LPARAM)L"Public");
    SendMessageW(gApp.hVisibilityCombo, CB_SETCURSEL, 0, 0);

    gApp.hChatEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hWnd, nullptr, hInst, nullptr);

    gApp.hSocialSearchEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hWnd, nullptr, hInst, nullptr);

    gApp.hSocialPrivateChatEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hWnd, nullptr, hInst, nullptr);

}

static void UpdateControlVisibility(AppState& app)
{
    bool showStartControls = (app.viewport.GetPhase() == ScreenPhase::StartMenu);

    bool showSocialPopup =
        app.authenticated &&
        app.socialPopupOpen &&
        (app.viewport.GetPhase() == ScreenPhase::StartMenu ||
            app.viewport.GetPhase() == ScreenPhase::WaitingRoom ||
            app.viewport.GetPhase() == ScreenPhase::Playing);

    bool showChatControls =
        (app.viewport.GetPhase() == ScreenPhase::WaitingRoom ||
            app.viewport.GetPhase() == ScreenPhase::Playing) &&
        !showSocialPopup;

    bool allowStartControls = showStartControls && !showSocialPopup;

    ShowWindow(app.hNameEdit, allowStartControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hNameEdit, allowStartControls ? TRUE : FALSE);

    ShowWindow(app.hRoomEdit, allowStartControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hRoomEdit, allowStartControls ? TRUE : FALSE);

    ShowWindow(app.hPlayerCountCombo, allowStartControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hPlayerCountCombo, allowStartControls ? TRUE : FALSE);

    ShowWindow(app.hPasswordEdit, allowStartControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hPasswordEdit, allowStartControls ? TRUE : FALSE);

    ShowWindow(app.hVisibilityCombo, allowStartControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hVisibilityCombo, allowStartControls ? TRUE : FALSE);

    ShowWindow(app.hChatEdit, showChatControls ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hChatEdit, showChatControls ? TRUE : FALSE);

    bool showSocialSearch = showSocialPopup && !app.socialChatViewOpen;
    bool showSocialPrivate = showSocialPopup && app.socialChatViewOpen;

    ShowWindow(app.hSocialSearchEdit, showSocialSearch ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hSocialSearchEdit, showSocialSearch ? TRUE : FALSE);

    ShowWindow(app.hSocialPrivateChatEdit, showSocialPrivate ? SW_SHOW : SW_HIDE);
    EnableWindow(app.hSocialPrivateChatEdit, showSocialPrivate ? TRUE : FALSE);
}

static std::wstring ToWide(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (required <= 0)
    {
        return L"";
    }

    std::wstring result(required, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], required);

    if (!result.empty() && result.back() == L'\0')
    {
        result.pop_back();
    }

    return result;
}

static std::string ReadWindowTextA(HWND hWnd)
{
    int length = GetWindowTextLengthA(hWnd);
    if (length <= 0)
    {
        return "";
    }

    std::string text(length + 1, '\0');
    GetWindowTextA(hWnd, &text[0], length + 1);
    text.resize(length);
    return text;
}

static int ReadEditInt(HWND hEdit, int fallbackValue)
{
    std::string text = ReadWindowTextA(hEdit);
    if (text.empty())
    {
        return fallbackValue;
    }

    try
    {
        return std::stoi(text);
    }
    catch (...)
    {
        return fallbackValue;
    }
}

static int ReadComboSelectedInt(HWND hCombo, int fallbackValue)
{
    int index = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR)
    {
        return fallbackValue;
    }

    wchar_t buffer[32]{};
    SendMessageW(hCombo, CB_GETLBTEXT, index, (LPARAM)buffer);

    try
    {
        return std::stoi(buffer);
    }
    catch (...)
    {
        return fallbackValue;
    }
}

static bool PointInRectEx(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right &&
        y >= rect.top && y < rect.bottom;
}

static void MarkUserActivity(AppState& app)
{
    app.lastInputTick = GetTickCount64();

    ScreenPhase phase = app.viewport.GetPhase();
    bool trackPhase =
        (phase == ScreenPhase::WaitingRoom || phase == ScreenPhase::Playing);

    if (!trackPhase || !app.authenticated)
    {
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (now - app.lastHeartbeatTick >= 1000)
    {
        app.bridge.SendHeartbeat();
        app.lastHeartbeatTick = now;
    }

    if (phase == ScreenPhase::Playing)
    {
        app.presencePopupVisible = false;
        app.localTimedOut = false;
    }
}

static RECT GetPresencePopupRect(const RECT& clientRect)
{
    int w = 360;
    int h = 170;
    int left = clientRect.left + ((clientRect.right - clientRect.left) - w) / 2;
    int top = clientRect.top + ((clientRect.bottom - clientRect.top) - h) / 2;
    return RECT{ left, top, left + w, top + h };
}

static RECT GetPresenceYesButtonRect(const RECT& popupRect)
{
    return RECT{
        popupRect.left + 110,
        popupRect.bottom - 52,
        popupRect.right - 110,
        popupRect.bottom - 16
    };
}

static RECT GetPresenceResumeButtonRect(const RECT& popupRect)
{
    return RECT{
        popupRect.left + 110,
        popupRect.bottom - 52,
        popupRect.right - 110,
        popupRect.bottom - 16
    };
}

static void DrawPresencePopup(HDC hdc, const RECT& clientRect, AppState& app)
{
    if (app.viewport.GetPhase() != ScreenPhase::Playing || !app.presencePopupVisible)
    {
        return;
    }

    RECT popup = GetPresencePopupRect(clientRect);

    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &popup, bg);
    DeleteObject(bg);

    FrameRect(hdc, &popup, (HBRUSH)GetStockObject(BLACK_BRUSH));

    RECT titleRect{ popup.left + 16, popup.top + 14, popup.right - 16, popup.top + 40 };
    DrawTextW(
        hdc,
        app.localTimedOut ? L"Disconnected" : L"Are you still there?",
        -1,
        &titleRect,
        DT_CENTER | DT_SINGLELINE
    );

    std::wstring body =
        app.localTimedOut
        ? L"You were disconnected due to inactivity. Click Resume to continue if the match is still running."
        : L"You have been idle for a while. Click Yes to keep playing.";

    RECT bodyRect{ popup.left + 18, popup.top + 48, popup.right - 18, popup.top + 108 };
    DrawTextW(hdc, body.c_str(), -1, &bodyRect, DT_WORDBREAK | DT_CENTER);

    if (app.localTimedOut)
    {
        app.resumeMatchButton.rect = GetPresenceResumeButtonRect(popup);
        app.resumeMatchButton.enabled = true;
        DrawButton(hdc, app.resumeMatchButton);
    }
    else
    {
        app.stayHereButton.rect = GetPresenceYesButtonRect(popup);
        app.stayHereButton.enabled = true;
        DrawButton(hdc, app.stayHereButton);
    }
}

static void UpdateIdleState(HWND hWnd, AppState& app)
{
    UNREFERENCED_PARAMETER(hWnd);

    if (!app.authenticated)
    {
        app.presencePopupVisible = false;
        app.localTimedOut = false;
        return;
    }

    if (app.viewport.GetPhase() != ScreenPhase::Playing)
    {
        app.presencePopupVisible = false;
        app.localTimedOut = false;
        return;
    }

    ULONGLONG now = GetTickCount64();
    ULONGLONG idleMs = now - app.lastInputTick;

    if (idleMs >= 85000)
    {
        app.presencePopupVisible = true;
        app.localTimedOut = true;
        app.statusText = "Disconnected due to inactivity. Click Yes to resume if the match is still running.";
    }
    else if (idleMs >= 55000)
    {
        app.presencePopupVisible = true;
        app.localTimedOut = false;
    }
    else
    {
        app.presencePopupVisible = false;
        app.localTimedOut = false;
    }
}

static bool RectsIntersect(const RECT& a, const RECT& b)
{
    RECT out{};
    return IntersectRect(&out, &a, &b) != FALSE;
}

static void ShowControlIfNotCovered(HWND hWnd, bool baseVisible, const RECT& popupRect)
{
    if (hWnd == nullptr)
    {
        return;
    }

    if (!baseVisible)
    {
        ShowWindow(hWnd, SW_HIDE);
        EnableWindow(hWnd, FALSE);
        return;
    }

    RECT ctrlRect{};
    GetWindowRect(hWnd, &ctrlRect);

    POINT tl{ ctrlRect.left, ctrlRect.top };
    POINT br{ ctrlRect.right, ctrlRect.bottom };

    HWND parent = GetParent(hWnd);
    if (parent != nullptr)
    {
        ScreenToClient(parent, &tl);
        ScreenToClient(parent, &br);
    }

    RECT localRect{ tl.x, tl.y, br.x, br.y };

    bool covered = RectsIntersect(localRect, popupRect);

    ShowWindow(hWnd, covered ? SW_HIDE : SW_SHOW);
    EnableWindow(hWnd, covered ? FALSE : TRUE);
}

static void DrawButton(HDC hdc, const UiButton& button, bool forestChrome)
{
    COLORREF fill;
    COLORREF borderColor;

    if (forestChrome)
    {
        fill = RGB(72, 118, 84);
        borderColor = RGB(38, 62, 44);

        if (button.completed)
        {
            fill = RGB(145, 150, 142);
            borderColor = RGB(80, 88, 80);
        }
        else if (!button.enabled)
        {
            fill = RGB(130, 162, 138);
            borderColor = RGB(70, 95, 76);
        }
    }
    else
    {
        fill = RGB(110, 165, 235);
        borderColor = RGB(128, 128, 128);

        if (button.completed)
        {
            fill = RGB(180, 180, 180);
        }
        else if (!button.enabled)
        {
            fill = RGB(170, 200, 235);
        }
    }

    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(hdc, &button.rect, brush);
    DeleteObject(brush);

    HBRUSH borderBrush = CreateSolidBrush(borderColor);
    FrameRect(hdc, &button.rect, borderBrush);
    DeleteObject(borderBrush);

    int btnH = button.rect.bottom - button.rect.top;
    int fontHeight = btnH / 2;
    if (fontHeight < 14) fontHeight = 14;
    if (fontHeight > 22) fontHeight = 22;

    HFONT buttonFont = CreateFontW(
        fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT oldFont = (HFONT)SelectObject(hdc, buttonFont);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    RECT textRect = button.rect;
    DrawTextW(
        hdc,
        button.text.c_str(),
        -1,
        &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB(0, 0, 0));

    SelectObject(hdc, oldFont);
    DeleteObject(buttonFont);
}

static void DrawStartMenu(HDC hdc, const RECT& clientRect, AppState& app)
{
    SetBkMode(hdc, TRANSPARENT);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int margin = max(20, clientW / 40);
    int gap = max(16, clientW / 50);
    int verticalGap = max(8, clientH / 60);

    int titleTop = max(30, clientH / 18);
    int titleHeight = max(48, clientH / 12);
    int contentTop = titleTop + titleHeight + max(30, clientH / 30);

    int leftCardW = (clientW - margin * 2 - gap) / 3;
    if (leftCardW < 280) leftCardW = 280;

    int rightAreaW = clientW - margin * 2 - gap - leftCardW;
    if (rightAreaW < 320) rightAreaW = 320;

    int leftX = margin;
    int rightX = leftX + leftCardW + gap;

    int topCardH = max(210, clientH / 4);
    int bottomCardH = max(170, clientH / 5);
    int leftCardH = topCardH + gap + bottomCardH;

    RECT leftCard = MakeRect(leftX, contentTop, leftCardW, leftCardH);
    RECT topCard = MakeRect(rightX, contentTop, rightAreaW, topCardH);
    RECT bottomCard = MakeRect(rightX, contentTop + topCardH + gap, rightAreaW, bottomCardH);

    DrawStartMenuBackground(hdc, clientRect);

    HFONT titleFont = CreateFontW(
        max(28, clientH / 16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT sectionFont = CreateFontW(
        max(18, clientH / 32), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT labelFont = CreateFontW(
        max(14, clientH / 45), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);

    RECT titleRect{ 0, titleTop, clientW, titleTop + titleHeight };
    RECT titleShadowRect = titleRect;
    OffsetRect(&titleShadowRect, 2, 2);
    SetTextColor(hdc, RGB(18, 32, 22));
    DrawTextW(hdc, L"Sudoku Rush", -1, &titleShadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB(248, 252, 246));
    DrawTextW(hdc, L"Sudoku Rush", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB(32, 48, 36));

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(88, 118, 92));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH cardBrush = CreateSolidBrush(RGB(255, 252, 245));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, cardBrush);

    RoundRect(hdc, leftCard.left, leftCard.top, leftCard.right, leftCard.bottom, 16, 16);
    RoundRect(hdc, topCard.left, topCard.top, topCard.right, topCard.bottom, 16, 16);
    RoundRect(hdc, bottomCard.left, bottomCard.top, bottomCard.right, bottomCard.bottom, 16, 16);

    SelectObject(hdc, oldBrush);
    DeleteObject(cardBrush);

    int pad = max(18, clientW / 60);

    SelectObject(hdc, sectionFont);

    RECT leftHeader{ leftCard.left + pad, leftCard.top + 16, leftCard.right - pad, leftCard.top + 50 };
    DrawTextW(hdc, L"Login", -1, &leftHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT topHeader{ topCard.left + pad, topCard.top + 16, topCard.right - pad, topCard.top + 50 };
    DrawTextW(hdc, L"Create a New Game", -1, &topHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT bottomHeader{ bottomCard.left + pad, bottomCard.top + 16, bottomCard.right - pad, bottomCard.top + 50 };
    DrawTextW(hdc, L"Join an Existing Game", -1, &bottomHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, labelFont);

    RECT userLabel{ leftCard.left + pad, leftCard.top + 60, leftCard.right - pad, leftCard.top + 96 };
    DrawTextW(hdc, L"Username", -1, &userLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT passLabel{ leftCard.left + pad, leftCard.top + 154, leftCard.right - pad, leftCard.top + 182 };
    DrawTextW(hdc, L"Password", -1, &passLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT playersLabel{ topCard.left + pad + 8, topCard.top + 60, topCard.right - pad, topCard.top + 96 };
    DrawTextW(hdc, L"Players", -1, &playersLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT roomLabel{ bottomCard.left + pad + 8, bottomCard.top + 56, bottomCard.right - pad, bottomCard.top + 84 };
    DrawTextW(hdc, L"Room ID", -1, &roomLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    app.signUpButton.enabled = !app.pendingRoomRequest;
    app.signInButton.enabled = !app.pendingRoomRequest;
    app.createButton.enabled = app.authenticated && !app.pendingRoomRequest;
    app.joinButton.enabled = app.authenticated && !app.pendingRoomRequest;

    DrawButton(hdc, app.signUpButton, true);
    DrawButton(hdc, app.signInButton, true);
    DrawButton(hdc, app.createButton, true);
    DrawButton(hdc, app.joinButton, true);

    int historyW = max(90, clientW / 10);
    int historyH = max(34, clientH / 16);
    int historyRightMargin = max(20, clientW / 30);
    int historyTop = max(20, clientH / 30);

    app.historyButton.rect = MakeRect(
        clientRect.right - historyRightMargin - historyW,
        historyTop,
        historyW,
        historyH
    );

    app.historyButton.enabled = app.authenticated && !app.pendingRoomRequest;
    DrawButton(hdc, app.historyButton, true);

    std::wstring connText;
    if (!app.bridge.IsConnected())
    {
        connText = L"UDP client not ready";
    }
    else if (!app.bridge.HasDiscoveredServer())
    {
        connText = L"Searching for LAN server...";
    }
    else
    {
        connText = L"LAN server discovered";
    }

    COLORREF oldTextColor = SetTextColor(hdc, RGB(255, 255, 255));

    TextOutW(hdc, leftCard.left + pad, leftCard.bottom - 42, connText.c_str(), (int)connText.size());

    std::wstring status = L"Status: " + ToWide(app.statusText);
    TextOutW(hdc, leftCard.left + pad, leftCard.bottom + 18, status.c_str(), (int)status.size());

    SetTextColor(hdc, oldTextColor);
    DrawPublicRoomList(hdc, clientRect, app, true);
    DrawSocialPopup(hdc, clientRect, app, true);

    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(sectionFont);
    DeleteObject(labelFont);
}

static void DrawPublicRoomList(HDC hdc, const RECT& clientRect, AppState& app, bool forestChrome)
{
    app.publicRoomJoinRects.clear();

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int margin = max(20, clientW / 40);
    int gap = max(16, clientH / 50);

    // align with right-side cards from start menu
    int leftCardW = (clientW - margin * 2 - gap) / 3;
    if (leftCardW < 280) leftCardW = 280;

    int rightX = margin + leftCardW + gap;
    int rightAreaW = clientW - margin * 2 - gap - leftCardW;
    if (rightAreaW < 320) rightAreaW = 320;

    int topY = max(520, clientH * 3 / 4);   // keep it in lower area
    int socialButtonReserve = 120;
    int width = rightAreaW - socialButtonReserve;
    if (width < 260) width = 260;
    int rowH = max(38, clientH / 18);
    int buttonW = 120;
    int buttonPad = max(8, clientW / 100);

    RECT titleRect{ rightX, topY, rightX + width, topY + 30 };
    if (forestChrome)
    {
        RECT trSh = titleRect;
        OffsetRect(&trSh, 1, 1);
        SetTextColor(hdc, RGB(12, 26, 18));
        DrawTextW(hdc, L"Public Available Rooms", -1, &trSh, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(244, 250, 242));
        DrawTextW(hdc, L"Public Available Rooms", -1, &titleRect, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(28, 42, 32));
    }
    else
    {
        DrawTextW(hdc, L"Public Available Rooms", -1, &titleRect, DT_LEFT | DT_SINGLELINE);
    }

    int y = topY + 40;

    if (app.publicRooms.empty())
    {
        RECT emptyRect{ rightX, y, rightX + width, y + rowH };
        if (forestChrome)
        {
            HBRUSH rowBg = CreateSolidBrush(RGB(255, 252, 245));
            FillRect(hdc, &emptyRect, rowBg);
            DeleteObject(rowBg);
            HBRUSH rowBorder = CreateSolidBrush(RGB(90, 118, 94));
            FrameRect(hdc, &emptyRect, rowBorder);
            DeleteObject(rowBorder);
        }
        DrawTextW(hdc, L"No public rooms available.", -1, &emptyRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    for (std::size_t i = 0; i < app.publicRooms.size(); ++i)
    {
        const PublicRoomInfo& room = app.publicRooms[i];

        RECT rowRect{ rightX, y, rightX + width, y + rowH };
        if (forestChrome)
        {
            HBRUSH rowBg = CreateSolidBrush(RGB(255, 252, 245));
            FillRect(hdc, &rowRect, rowBg);
            DeleteObject(rowBg);
            HBRUSH rowBorder = CreateSolidBrush(RGB(90, 118, 94));
            FrameRect(hdc, &rowRect, rowBorder);
            DeleteObject(rowBorder);
        }
        else
        {
            FrameRect(hdc, &rowRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
        }

        std::wstringstream text;
        text << L"Room " << room.roomId
            << L" | Host: " << ToWide(room.hostName)
            << L" | " << room.currentPlayers << L"/" << room.maxPlayers;

        std::wstring ws = text.str();

        RECT textRect
        {
            rowRect.left + 12,
            rowRect.top,
            rowRect.right - buttonW - 24,
            rowRect.bottom
        };

        DrawTextW(
            hdc,
            ws.c_str(),
            -1,
            &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
        );

        RECT joinRect
        {
            textRect.right + 8,
            rowRect.top + buttonPad / 2,
            textRect.right + 8 + buttonW,
            rowRect.bottom - buttonPad / 2
        };
        app.publicRoomJoinRects.push_back(joinRect);

        UiButton joinBtn{};
        joinBtn.rect = joinRect;
        joinBtn.text = L"Join";
        joinBtn.enabled = true;
        DrawButton(hdc, joinBtn, forestChrome);

        y += rowH + max(8, clientH / 80);
    }
}

static void DrawSocialPopup(HDC hdc, const RECT& clientRect, AppState& app, bool forestChrome)
{
    if (!app.authenticated)
    {
        return;
    }

    app.socialLauncherButton.rect = GetSocialLauncherRect(clientRect);
    app.socialLauncherButton.text = app.socialPopupOpen ? L"X" : L"+";
    app.socialLauncherButton.enabled = true;
    DrawButton(hdc, app.socialLauncherButton, forestChrome);

    if (!app.socialPopupOpen)
    {
        return;
    }

    RECT popup = GetSocialPopupRect(clientRect);

    HBRUSH popupBrush = CreateSolidBrush(forestChrome ? RGB(255, 252, 246) : RGB(252, 252, 252));
    FillRect(hdc, &popup, popupBrush);
    DeleteObject(popupBrush);
    if (forestChrome)
    {
        HBRUSH frameBr = CreateSolidBrush(RGB(86, 115, 90));
        FrameRect(hdc, &popup, frameBr);
        DeleteObject(frameBr);
    }
    else
    {
        FrameRect(hdc, &popup, (HBRUSH)GetStockObject(GRAY_BRUSH));
    }

    if (app.socialChatViewOpen)
    {
        DrawSocialChatView(hdc, popup, app, forestChrome);
    }
    else
    {
        DrawSocialHome(hdc, popup, app, forestChrome);
    }
}

static void DrawSocialHome(HDC hdc, const RECT& popup, AppState& app, bool forestChrome)
{
    app.pendingRequestRects.clear();
    app.friendRects.clear();

    RECT titleRect{ popup.left + 16, popup.top + 12, popup.right - 16, popup.top + 34 };
    DrawTextW(hdc, L"Friends & Chat", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    RECT searchRect = GetSocialSearchRect(popup);
    RECT searchBtnRect = GetSocialSearchButtonRect(popup);
    RECT addBtnRect = GetSocialAddFriendButtonRect(popup);

    MoveWindow(
        app.hSocialSearchEdit,
        searchRect.left,
        searchRect.top,
        searchRect.right - searchRect.left,
        searchRect.bottom - searchRect.top,
        FALSE
    );

    ShowWindow(app.hSocialSearchEdit, SW_SHOW);
    ShowWindow(app.hSocialPrivateChatEdit, SW_HIDE);

    app.socialSearchButton.rect = searchBtnRect;
    app.socialAddFriendButton.rect = addBtnRect;

    DrawButton(hdc, app.socialSearchButton, forestChrome);
    DrawButton(hdc, app.socialAddFriendButton, forestChrome);

    RECT statusRect{ popup.left + 16, popup.top + 126, popup.right - 16, popup.top + 146 };
    std::wstring socialStatus = ToWide(app.socialStatusText);

    COLORREF oldColor = SetTextColor(hdc, RGB(130, 130, 130));
    DrawTextW(hdc, socialStatus.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetTextColor(hdc, oldColor);

    RECT pendingTitle{ popup.left + 16, popup.top + 160, popup.right - 16, popup.top + 180 };
    DrawTextW(hdc, L"Pending Requests:", -1, &pendingTitle, DT_LEFT | DT_SINGLELINE);

    int y = popup.top + 185;
    for (std::size_t i = 0; i < app.pendingFriendRequests.size() && i < 5; ++i)
    {
        RECT r{ popup.left + 16, y, popup.right - 16, y + 22 };
        app.pendingRequestRects.push_back(r);

        std::wstring line = ToWide(app.pendingFriendRequests[i] + "  [Click to accept]");
        DrawTextW(hdc, line.c_str(), -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 24;
    }

    RECT friendsTitle{ popup.left + 16, popup.top + 310, popup.right - 16, popup.top + 332 };
    DrawTextW(hdc, L"Friends:", -1, &friendsTitle, DT_LEFT | DT_SINGLELINE);

    y = popup.top + 336;
    for (std::size_t i = 0; i < app.friendsList.size() && i < 10; ++i)
    {
        RECT r{ popup.left + 16, y, popup.right - 16, y + 24 };
        app.friendRects.push_back(r);

        std::wstring line = ToWide(app.friendsList[i]);
        DrawTextW(hdc, line.c_str(), -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 26;
    }
}

static void DrawSocialChatView(HDC hdc, const RECT& popup, AppState& app, bool forestChrome)
{
    RECT titleRect{ popup.left + 16, popup.top + 12, popup.right - 120, popup.top + 34 };
    std::wstring title = app.selectedFriendName.empty()
        ? L"Private Chat"
        : (L"Chat with " + ToWide(app.selectedFriendName));
    DrawTextW(hdc, title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    app.socialBackButton.rect = GetSocialBackButtonRect(popup);
    app.socialBackButton.enabled = true;
    DrawButton(hdc, app.socialBackButton, forestChrome);

    RECT messagesRect = GetSocialChatMessagesRect(popup);
    if (forestChrome)
    {
        HBRUSH msgBr = CreateSolidBrush(RGB(95, 120, 98));
        FrameRect(hdc, &messagesRect, msgBr);
        DeleteObject(msgBr);
    }
    else
    {
        FrameRect(hdc, &messagesRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
    }

    RECT inputRect = GetSocialPrivateInputRect(popup);
    RECT sendRect = GetSocialPrivateSendRect(popup);

    MoveWindow(
        app.hSocialPrivateChatEdit,
        inputRect.left,
        inputRect.top,
        inputRect.right - inputRect.left,
        inputRect.bottom - inputRect.top,
        FALSE
    );

    ShowWindow(app.hSocialPrivateChatEdit, SW_SHOW);
    ShowWindow(app.hSocialSearchEdit, SW_HIDE);

    app.socialPrivateSendButton.rect = sendRect;
    DrawButton(hdc, app.socialPrivateSendButton, forestChrome);

    int lineH = 20;
    int y = messagesRect.top + 10;
    int maxLines = max(1, (messagesRect.bottom - messagesRect.top - 20) / lineH);

    std::vector<std::wstring> visibleMessages;

    for (const PrivateChatMessage& msg : app.privateChatMessages)
    {
        bool isThisThread =
            (msg.senderName == app.selectedFriendName) ||
            (msg.receiverName == app.selectedFriendName);

        if (!isThisThread)
        {
            continue;
        }

        std::string prefix = (msg.senderName == app.selectedFriendName) ? "Them: " : "You: ";
        visibleMessages.push_back(ToWide(prefix + msg.text));
    }

    int startIndex = 0;
    if ((int)visibleMessages.size() > maxLines)
    {
        startIndex = (int)visibleMessages.size() - maxLines;
    }

    for (int i = startIndex; i < (int)visibleMessages.size(); ++i)
    {
        RECT msgRect{ messagesRect.left + 10, y, messagesRect.right - 10, y + lineH };
        DrawTextW(
            hdc,
            visibleMessages[i].c_str(),
            -1,
            &msgRect,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS
        );
        y += lineH;
    }
}

static void DrawRoomChat(HDC hdc, const RECT& clientRect, AppState& app)
{
    RECT panel = GetChatPanelRect(clientRect);
    FrameRect(hdc, &panel, (HBRUSH)GetStockObject(GRAY_BRUSH));

    RECT titleRect{ panel.left + 8, panel.top + 6, panel.right - 8, panel.top + 28 };
    DrawTextW(hdc, L"Room Chat", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    RECT inputRect = GetChatInputRect(clientRect);
    RECT sendRect = GetChatSendButtonRect(clientRect);
    app.sendChatButton.rect = sendRect;

    MoveWindow(
        app.hChatEdit,
        inputRect.left,
        inputRect.top,
        inputRect.right - inputRect.left,
        inputRect.bottom - inputRect.top,
        FALSE
    );

    
    int y = panel.top + 34;
    int lineH = 20;
    int maxLines = max(1, (inputRect.top - y - 6) / lineH);

    int start = 0;
    if ((int)app.roomChatMessages.size() > maxLines)
    {
        start = (int)app.roomChatMessages.size() - maxLines;
    }

    for (int i = start; i < (int)app.roomChatMessages.size(); ++i)
    {
        const ChatMessage& msg = app.roomChatMessages[i];
        std::wstring line = ToWide(msg.senderName + ": " + msg.text);

        RECT lineRect{ panel.left + 8, y, panel.right - 8, y + lineH };
        DrawTextW(hdc, line.c_str(), -1, &lineRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += lineH;
    }

    DrawButton(hdc, app.sendChatButton);
}

static void DrawWaitingRoom(HDC hdc, const RECT& clientRect, AppState& app)
{
    SetBkMode(hdc, TRANSPARENT);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int marginX = max(30, clientW / 20);
    int topY = max(20, clientH / 30);
    int lineGap = max(24, clientH / 30);
    int sectionGap = max(34, clientH / 22);

    HFONT titleFont = CreateFontW(
        max(22, clientH / 18), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT textFont = CreateFontW(
        max(14, clientH / 40), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);

    RECT titleRect{ marginX, topY, clientRect.right - marginX, topY + 40 };
    DrawTextW(hdc, L"Waiting Room", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, textFont);

    int y = topY + 55;

    std::wstringstream ss;
    ss << L"Room ID: " << app.bridge.GetAssignedRoomId();
    std::wstring roomText = ss.str();
    TextOutW(hdc, marginX, y, roomText.c_str(), (int)roomText.size());
    y += lineGap;

    ss.str(L"");
    ss.clear();
    ss << L"Player ID: " << app.bridge.GetAssignedPlayerId();
    std::wstring playerText = ss.str();
    TextOutW(hdc, marginX, y, playerText.c_str(), (int)playerText.size());
    y += lineGap;

    const RoomSnapshot& snapshot = app.waitingRoom.GetSnapshot();

    RECT chatPanel = GetChatPanelRect(clientRect);
    int textRightLimit = chatPanel.left - 20;

    ss.str(L"");
    ss.clear();
    ss << L"Players in room: " << snapshot.players.size() << L"/" << snapshot.config.maxPlayers;
    std::wstring countText = ss.str();
    TextOutW(hdc, marginX, y, countText.c_str(), (int)countText.size());
    y += sectionGap;

    for (std::size_t i = 0; i < snapshot.players.size(); ++i)
    {
        const PlayerScoreInfo& player = snapshot.players[i];

        std::wstringstream line;
        line << L"Player " << (i + 1)
            << L" - " << ToWide(player.playerName)
            << L" - " << (player.ready ? L"READY" : L"NOT READY");

        std::wstring text = line.str();
        RECT lineRect{ marginX, y, textRightLimit, y + lineGap };
        DrawTextW(hdc, text.c_str(), -1, &lineRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += lineGap;
    }

    int buttonGap = max(12, clientW / 60);
    int buttonW = max(120, clientW / 6);
    int buttonH = max(36, clientH / 18);
    int buttonTop = y + 16;

    app.readyButton.rect = MakeRect(marginX, buttonTop, buttonW, buttonH);
    app.quitButton.rect = MakeRect(marginX + buttonW + buttonGap, buttonTop, buttonW, buttonH);

    app.readyButton.text = app.waitingRoom.IsLocalReady() ? L"Unready" : L"Ready";
    DrawButton(hdc, app.readyButton);
    DrawButton(hdc, app.quitButton);

    std::wstring status = L"Status: " + ToWide(app.statusText);
    TextOutW(hdc, marginX, buttonTop + buttonH + 26, status.c_str(), (int)status.size());

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(textFont);
}

static void DrawPuzzleArena(HDC hdc, const RECT& clientRect, AppState& app)
{
    PaintedBoard painted = app.gridPainter.BuildBoardModel(
        app.viewport.GetBoard(),
        app.viewport.IsNotesModeEnabled());

    int highlightedDigit = GetHighlightedDigit(painted);

    RECT boardRect = GetResponsiveBoardRect(clientRect);
    RECT sidebarRect = GetSidebarRect(clientRect);
    app.notesButton.rect = GetNotesButtonRect(clientRect);
    app.hintButton.rect = GetHintButtonRect(clientRect);

    int boardSize = boardRect.right - boardRect.left;
    int cellW = boardSize / RuleBook::BOARD_SIZE;
    int cellH = boardSize / RuleBook::BOARD_SIZE;

    // force the board to match the exact drawable grid size
    boardRect.right = boardRect.left + cellW * RuleBook::BOARD_SIZE;
    boardRect.bottom = boardRect.top + cellH * RuleBook::BOARD_SIZE;

    SetBkMode(hdc, TRANSPARENT);

    RECT titleRect{ boardRect.left, 40, boardRect.left + 250, 70 };
    DrawTextW(hdc, L"Sudoku Match", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    HintState hint = app.viewport.GetHintState();

    app.notesButton.text = app.viewport.IsNotesModeEnabled() ? L"Pencil: ON" : L"Pencil: OFF";

    if (hint.unlocked && hint.availableCount > 0)
    {
        app.hintButton.enabled = true;
        app.hintButton.text = L"Hint";
    }
    else
    {
        app.hintButton.enabled = false;
        app.hintButton.text = L"Hint";
    }

    DrawButton(hdc, app.notesButton);
    DrawButton(hdc, app.hintButton);

    int leaderboardX = sidebarRect.left;
    int leaderboardY = app.hintButton.rect.bottom + 24;

    TextOutW(hdc, leaderboardX, leaderboardY, L"Leaderboard", 11);
    leaderboardY += 30;

    const std::vector<PlayerScoreInfo>& leaderboard = app.viewport.GetLeaderboard();

    for (std::size_t i = 0; i < leaderboard.size(); ++i)
    {
        std::wstringstream line;
        line << (i + 1) << L". "
            << ToWide(leaderboard[i].playerName)
            << L"  Score: " << leaderboard[i].score;

        std::wstring text = line.str();
        TextOutW(hdc, leaderboardX, leaderboardY, text.c_str(), (int)text.size());
        leaderboardY += 24;
    }

    for (const PaintedCell& cell : painted.cells)
    {
        RECT cellRect
        {
            boardRect.left + cell.col * cellW,
            boardRect.top + cell.row * cellH,
            boardRect.left + (cell.col + 1) * cellW,
            boardRect.top + (cell.row + 1) * cellH
        };

        COLORREF fillColor = RGB(255, 255, 255);

        if (cell.hasMainValue &&
            highlightedDigit != 0 &&
            cell.mainValue == highlightedDigit &&
            !cell.isSelected)
        {
            fillColor = RGB(205, 205, 205);   // same number highlight
        }

        if (cell.isFixed)
        {
            fillColor = RGB(245, 245, 245);
        }

        if (cell.firstSolver == CellOwner::Player1)
        {
            fillColor = RGB(255, 235, 210);   // light orange
        }
        else if (cell.firstSolver == CellOwner::Player2)
        {
            fillColor = RGB(235, 225, 255);   // light purple
        }
        else if (cell.firstSolver == CellOwner::Player3)
        {
            fillColor = RGB(220, 235, 255);   // light blue
        }

        if (cell.hasMainValue &&
            highlightedDigit != 0 &&
            cell.mainValue == highlightedDigit &&
            !cell.isSelected)
        {
            fillColor = RGB(205, 205, 205);   // keep number highlight visible
        }

        if (cell.isSelected)
        {
            fillColor = RGB(230, 240, 255);   // selected cell always strongest
        }

        HBRUSH fillBrush = CreateSolidBrush(fillColor);
        FillRect(hdc, &cellRect, fillBrush);
        DeleteObject(fillBrush);

        FrameRect(hdc, &cellRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

        if (cell.hasMainValue)
        {
            COLORREF textColor = RGB(0, 0, 0);

            if (cell.isFixed)
            {
                textColor = RGB(60, 60, 60);
            }
            else if (cell.isWrongTemporary)
            {
                textColor = RGB(200, 0, 0);   // wrong = red text
            }
            else
            {
                textColor = RGB(0, 140, 0);   // correct entered = green text
            }

            SetTextColor(hdc, textColor);

            wchar_t digit[2]{};
            digit[0] = static_cast<wchar_t>(L'0' + cell.mainValue);
            digit[1] = L'\0';

            DrawTextW(
                hdc,
                digit,
                -1,
                &cellRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, RGB(0, 0, 0));
        }


        else if (!cell.notes.empty())
        {
            int noteIndex = 0;
            for (int note : cell.notes)
            {
                int miniRow = noteIndex / 3;
                int miniCol = noteIndex % 3;

                RECT noteRect
                {
                    cellRect.left + miniCol * (cellW / 3),
                    cellRect.top + miniRow * (cellH / 3),
                    cellRect.left + (miniCol + 1) * (cellW / 3),
                    cellRect.top + (miniRow + 1) * (cellH / 3)
                };

                wchar_t digit[2]{};
                digit[0] = static_cast<wchar_t>(L'0' + note);
                digit[1] = L'\0';

                DrawTextW(
                    hdc,
                    digit,
                    -1,
                    &noteRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                ++noteIndex;
                if (noteIndex >= 9)
                {
                    break;
                }
            }
        }
    }


    for (int i = 0; i <= RuleBook::BOARD_SIZE; ++i)
    {
        int x = boardRect.left + i * cellW;
        int y = boardRect.top + i * cellH;

        HPEN pen = CreatePen(PS_SOLID, (i % 3 == 0) ? 3 : 1, RGB(0, 0, 0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        MoveToEx(hdc, x, boardRect.top, nullptr);
        LineTo(hdc, x, boardRect.bottom);

        MoveToEx(hdc, boardRect.left, y, nullptr);
        LineTo(hdc, boardRect.right, y);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    const LocalBoardState& board = app.viewport.GetBoard();

    // button layout
    int gap = 6;
    int totalWidth = boardRect.right - boardRect.left;
    int btnW = (totalWidth - gap * 8) / 9;
    int top = boardRect.bottom + 20;

    for (int i = 0; i < 9; ++i)
    {
        int digit = i + 1;

        RECT r{
            boardRect.left + i * (btnW + gap),
            top,
            boardRect.left + i * (btnW + gap) + btnW,
            top + 40
        };

        app.digitButtons[i].rect = r;

        int count = 0;
        for (int row = 0; row < 9; ++row)
        {
            for (int col = 0; col < 9; ++col)
            {
                if (board.cells[row][col].value == digit)
                {
                    count++;
                }
            }
        }

        bool completed = (count >= 9);

        app.digitButtons[i].completed = completed;
        app.digitButtons[i].enabled = !completed;

        DrawButton(hdc, app.digitButtons[i]);
    }

    std::wstringstream hintText;
    hintText << L"Hint: " << (hint.unlocked ? L"Unlocked" : L"Locked")
        << L" (" << hint.availableCount << L")";
    std::wstring hintStr = hintText.str();
    TextOutW(hdc, leaderboardX, leaderboardY + 10, hintStr.c_str(), (int)hintStr.size());

    if (hint.unlocked && hint.availableCount > 0)
    {
        std::wstringstream line1;
        line1 << L"Unlocked (" << hint.availableCount << L")";

        std::wstring line1Text = line1.str();
        std::wstring line2 = L"Click a cell, then click Hint.";
        std::wstring line3 = L"Each use consumes 1 hint.";

        TextOutW(hdc, leaderboardX, leaderboardY + 35, line1Text.c_str(), (int)line1Text.size());
        TextOutW(hdc, leaderboardX, leaderboardY + 58, line2.c_str(), (int)line2.size());
        TextOutW(hdc, leaderboardX, leaderboardY + 81, line3.c_str(), (int)line3.size());
    }

}

static int GetHighlightedDigit(const PaintedBoard& painted)
{
    for (const PaintedCell& cell : painted.cells)
    {
        if (cell.isSelected && cell.hasMainValue)
        {
            return cell.mainValue;
        }
    }

    return 0;
}

static void DrawRoundSummary(HDC hdc, const RECT& clientRect, AppState& app)
{

    SetBkMode(hdc, TRANSPARENT);

    RECT titleRect{ 40, 20, 400, 50 };
    DrawTextW(hdc, L"Round Summary", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    std::wstring resultText = ToWide(app.roundSummary.GetResultText());
    if (resultText.empty())
    {
        resultText = L"Round finished.";
    }

    RECT resultRect{ 40, 60, 700, 90 };
    DrawTextW(hdc, resultText.c_str(), -1, &resultRect, DT_LEFT | DT_SINGLELINE);

    TextOutW(hdc, 40, 110, L"Final Leaderboard", 17);

    const std::vector<PlayerScoreInfo>& leaderboard = app.roundSummary.GetFinalLeaderboard();

    int y = 145;
    for (std::size_t i = 0; i < leaderboard.size(); ++i)
    {
        std::wstringstream line;
        line << (i + 1) << L". "
            << ToWide(leaderboard[i].playerName)
            << L"   Score: " << leaderboard[i].score;

        if (leaderboard[i].finished)
        {
            line << L"   Finished";
        }

        std::wstring text = line.str();
        TextOutW(hdc, 40, y, text.c_str(), (int)text.size());
        y += 28;
    }

    std::wstring playAgainText =
        app.roundSummary.WantsPlayAgain()
        ? L"Play Again: YES"
        : L"Play Again: NO";

    TextOutW(hdc, 40, y + 20, playAgainText.c_str(), (int)playAgainText.size());
    TextOutW(hdc, 40, y + 50, L"Press P to toggle Play Again", 29);
    app.roundSummaryHomeButton.rect = MakeRect(clientRect.right - 150, 40, 100, 36);
    app.roundSummaryHomeButton.enabled = true;
    DrawButton(hdc, app.roundSummaryHomeButton);
}

static void DrawHistoryScreen(HDC hdc, const RECT& clientRect, AppState& app)
{
    SetBkMode(hdc, TRANSPARENT);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    HFONT titleFont = CreateFontW(
        max(22, clientH / 18), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT headerFont = CreateFontW(
        max(13, clientH / 38), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT rowFont = CreateFontW(
        max(12, clientH / 42), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);

    int marginX = max(20, clientW / 25);
    int titleTop = max(18, clientH / 30);

    RECT titleRect{ marginX, titleTop, clientRect.right - marginX, titleTop + max(35, clientH / 14) };
    DrawTextW(hdc, L"Match History", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    app.backButton.rect = GetHistoryBackButtonRect(clientRect);
    DrawButton(hdc, app.backButton);

    RECT contentRect = GetHistoryContentRect(clientRect);

    int headerHeight = max(34, clientH / 18);
    int rowHeight = max(42, clientH / 14);
    int rowGap = max(8, clientH / 100);

    const int totalColumns = 7;

    RECT headerRow{ contentRect.left, contentRect.top, contentRect.right, contentRect.top + headerHeight };

    HBRUSH headerBrush = CreateSolidBrush(RGB(235, 235, 235));
    FillRect(hdc, &headerRow, headerBrush);
    DeleteObject(headerBrush);

    FrameRect(hdc, &headerRow, (HBRUSH)GetStockObject(GRAY_BRUSH));

    SelectObject(hdc, headerFont);

    const wchar_t* headers[totalColumns] =
    {
        L"",
        L"Date / Time",
        L"Score",
        L"Wrong",
        L"Hints",
        L"Time / Avg",
        L"Rank"
    };

    for (int i = 0; i < totalColumns; ++i)
    {
        RECT cell = GetHistoryCellRect(contentRect, contentRect.top, headerHeight, i, totalColumns);
        DrawTextW(hdc, headers[i], -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(hdc, rowFont);

    int y = contentRect.top + headerHeight + rowGap;

    if (app.historyRows.empty())
    {
        RECT emptyRect{ contentRect.left, y, contentRect.right, y + rowHeight };
        DrawTextW(hdc, L"No history found.", -1, &emptyRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        for (std::size_t i = 0; i < app.historyRows.size(); ++i)
        {
            const MatchHistoryRow& row = app.historyRows[i];

            RECT rowRect{ contentRect.left, y, contentRect.right, y + rowHeight };

            HBRUSH rowBrush = CreateSolidBrush((i % 2 == 0) ? RGB(248, 248, 248) : RGB(238, 238, 238));
            FillRect(hdc, &rowRect, rowBrush);
            DeleteObject(rowBrush);

            FrameRect(hdc, &rowRect, (HBRUSH)GetStockObject(GRAY_BRUSH));

            std::wstring playedAtText = ToWide(row.playedAt);

            std::wstringstream scoreText;
            scoreText << row.finalScore;

            std::wstringstream wrongText;
            wrongText << row.wrongAttempts;

            std::wstringstream hintsText;
            hintsText << row.hintsUsed;

            std::wstringstream timeAvgText;
            timeAvgText << row.totalTimeSeconds << L"s / " << row.averageMoveSeconds << L"s";

            std::wstringstream rankText;
            rankText << row.finalRank << L"/" << row.totalPlayers;

            std::wstring values[totalColumns] =
            {
                L"",
                playedAtText,
                scoreText.str(),
                wrongText.str(),
                hintsText.str(),
                timeAvgText.str(),
                rankText.str()
            };

            for (int col = 0; col < totalColumns; ++col)
            {
                RECT cell = GetHistoryCellRect(contentRect, y, rowHeight, col, totalColumns);

                if (col == 0)
                {
                    HICON iconToDraw = (row.result == "WIN") ? app.hWinCrownIcon : app.hLoseCrownIcon;

                    if (iconToDraw != nullptr)
                    {
                        int iconSize = min(
                            (cell.right - cell.left) - 10,
                            (cell.bottom - cell.top) - 10
                        );

                        if (iconSize < 16)
                        {
                            iconSize = 16;
                        }

                        int iconX = cell.left + ((cell.right - cell.left) - iconSize) / 2;
                        int iconY = cell.top + ((cell.bottom - cell.top) - iconSize) / 2;

                        DrawIconEx(
                            hdc,
                            iconX,
                            iconY,
                            iconToDraw,
                            iconSize,
                            iconSize,
                            0,
                            nullptr,
                            DI_NORMAL
                        );
                    }
                }
                else
                {
                    SetTextColor(hdc, RGB(0, 0, 0));

                    DrawTextW(
                        hdc,
                        values[col].c_str(),
                        -1,
                        &cell,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
                    );
                }
            }

            SetTextColor(hdc, RGB(0, 0, 0));

            y += rowHeight + rowGap;

            if (y + rowHeight > contentRect.bottom)
            {
                break;
            }
        }
    }

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(headerFont);
    DeleteObject(rowFont);
}
static void DrawCurrentScreen(HDC hdc, const RECT& clientRect, AppState& app)
{
    switch (app.viewport.GetPhase())
    {
    case ScreenPhase::StartMenu:
        DrawStartMenu(hdc, clientRect, app);
        break;

    case ScreenPhase::WaitingRoom:
        DrawWaitingRoom(hdc, clientRect, app);
        DrawSocialPopup(hdc, clientRect, app, false);
        break;

    case ScreenPhase::Countdown:
    {
        RECT r{ 40, 40, 400, 80 };
        DrawTextW(hdc, L"Countdown starting...", -1, &r, DT_LEFT | DT_SINGLELINE);
        break;
    }

    case ScreenPhase::Playing:
        DrawPuzzleArena(hdc, clientRect, app);
        DrawSocialPopup(hdc, clientRect, app, false);
        DrawPresencePopup(hdc, clientRect, app);
        break;

    case ScreenPhase::RoundSummary:
        DrawRoundSummary(hdc, clientRect, app);
        break;

    case ScreenPhase::History:
        DrawHistoryScreen(hdc, clientRect, app);
        break;


    default:
        break;
    }
}

static void ProcessIncomingMessages(AppState& app)
{
    while (app.bridge.HasPendingMessage())
    {
        BridgeMessage msg = app.bridge.PopNextMessage();

        switch (msg.kind)
        {

        case PacketKind::AuthResponse:
            if (msg.authResponse.success)
            {
                app.authenticated = true;
                app.statusText = msg.authResponse.text;
                app.socialStatusText = "Chats are session-based.";
                app.bridge.SendRequestFriendsList();
                app.bridge.SendRequestPendingFriendRequests();
            }
            else
            {
                app.authenticated = false;
                app.statusText = msg.authResponse.text;
            }
            app.pendingRoomRequest = false;
            break;

        case PacketKind::RoomJoinedResponse:
            if (msg.roomJoin.success)
            {
                app.roomChatMessages.clear();
                app.viewport.SetPhase(ScreenPhase::WaitingRoom);
                app.lastInputTick = GetTickCount64();
                app.presencePopupVisible = false;
                app.localTimedOut = false;
                app.statusText = msg.roomJoin.text + " Room ID: " + std::to_string(msg.roomJoin.roomId);
            }
            else
            {
                app.viewport.SetPhase(ScreenPhase::StartMenu);
                app.statusText = msg.roomJoin.text;
            }
            app.pendingRoomRequest = false;
            break;

        case PacketKind::WaitingRoomUpdate:
            app.waitingRoom.ApplyRoomSnapshot(msg.roomSnapshot);
            app.viewport.SetPhase(ScreenPhase::WaitingRoom);
            break;

        case PacketKind::LeaveRoomResponse:
            app.presencePopupVisible = false;
            app.localTimedOut = false;
            app.bridge.ClearRoomAssignment();
            app.waitingRoom = WaitingRoom{};
            app.publicRooms.clear();
            app.publicRoomJoinRects.clear();
            app.roomChatMessages.clear();
            app.viewport.SetPhase(ScreenPhase::StartMenu);
            app.statusText = msg.text.empty() ? "Left room." : msg.text;
            app.pendingRoomRequest = false;
            app.bridge.SendRequestPublicRoomList();
            break;

        case PacketKind::RoomClosedNotice:
            app.presencePopupVisible = false;
            app.localTimedOut = false;
            app.bridge.ClearRoomAssignment();
            app.waitingRoom = WaitingRoom{};
            app.publicRooms.clear();
            app.publicRoomJoinRects.clear();
            app.roomChatMessages.clear();
            app.viewport.SetPhase(ScreenPhase::StartMenu);
            app.statusText = msg.text.empty() ? "Room closed." : msg.text;
            app.pendingRoomRequest = false;
            app.bridge.SendRequestPublicRoomList();
            break;

        case PacketKind::RoomChatMessageReceived:
            app.roomChatMessages.push_back(msg.chatMessage);
            if (app.roomChatMessages.size() > 100)
            {
                app.roomChatMessages.erase(app.roomChatMessages.begin());
            }
            break;

        case PacketKind::CountdownStarted:
            app.viewport.SetPhase(ScreenPhase::Countdown);
            break;

        case PacketKind::MatchStarted:
            if (app.bridge.GetAssignedRoomId() != 0)
            {
                app.viewport.ResetForNewMatch();
                app.viewport.ApplyPuzzle(msg.puzzleData);
                app.viewport.SetPhase(ScreenPhase::Playing);
                app.lastInputTick = GetTickCount64();
                app.presencePopupVisible = false;
                app.localTimedOut = false;
                app.statusText = "Match started.";
            }
            break;
    break;

        case PacketKind::CellCheckedResponse:
            app.viewport.ApplyCellResult(msg.cellResult);
            break;

        case PacketKind::CellClaimUpdateNotice:
            app.viewport.ApplyCellClaimUpdate(msg.cellClaim);
            break;

        case PacketKind::LeaderboardUpdate:
            app.viewport.UpdateLeaderboard(msg.leaderboard);
            app.roundSummary.UpdateFinalLeaderboard(msg.leaderboard);

            if (app.viewport.GetPhase() == ScreenPhase::Playing && app.localTimedOut)
            {
                app.localTimedOut = false;
                app.presencePopupVisible = false;
                app.lastInputTick = GetTickCount64();
                app.statusText = "Resume successful.";
            }
            break;

        case PacketKind::HintUnlockedNotice:
            app.viewport.SetHintState(msg.hintState);

            if (app.viewport.GetPhase() == ScreenPhase::Playing && app.localTimedOut)
            {
                app.localTimedOut = false;
                app.presencePopupVisible = false;
                app.lastInputTick = GetTickCount64();
            }
            break;

        case PacketKind::HistoryResponse:
            app.historyRows = msg.historyRows;
            app.viewport.SetPhase(ScreenPhase::History);
            app.statusText = "History loaded.";
            break;

        case PacketKind::PublicRoomListResponse:
            app.publicRooms = msg.publicRooms;
            break;

        case PacketKind::SearchUserResponse:
            if (msg.userSearchResult.found)
            {
                app.searchedUsername = msg.userSearchResult.username;
                app.socialStatusText = "User found: " + msg.userSearchResult.username;
            }
            else
            {
                app.searchedUsername.clear();
                app.socialStatusText = msg.userSearchResult.text;
            }
            break;

        case PacketKind::FriendRequestReceived:
            app.pendingFriendRequests.push_back(msg.text);
            app.socialStatusText = "New friend request from " + msg.text;
            break;

        case PacketKind::FriendRequestResponse:
            app.socialStatusText = msg.text;
            app.statusText = msg.text;
            app.bridge.SendRequestFriendsList();
            app.bridge.SendRequestPendingFriendRequests();
            break;

        case PacketKind::FriendsListResponse:
            app.friendsList.clear();
            for (const FriendInfo& item : msg.friendList)
            {
                app.friendsList.push_back(item.username);
            }
            break;

        case PacketKind::PendingFriendRequestsResponse:
            app.pendingFriendRequests.clear();
            for (const FriendInfo& item : msg.pendingFriendList)
            {
                app.pendingFriendRequests.push_back(item.username);
            }
            break;

        case PacketKind::PrivateMessageReceived:
            app.privateChatMessages.push_back(msg.privateChatMessage);
            app.socialStatusText = "PM from " + msg.privateChatMessage.senderName;
            break;

        case PacketKind::RoundFinishedNotice:
            app.roundSummary.SetResultText(msg.text);
            app.viewport.SetPhase(ScreenPhase::RoundSummary);
            app.statusText = msg.text;
            break;

        case PacketKind::ErrorNotice:
            app.statusText = msg.text;
            app.pendingRoomRequest = false;

            if (app.localTimedOut)
            {
                app.presencePopupVisible = true;
            }
            break;

        default:
            break;
        }
    }
}

static void HandleStartMenuClick(HWND hWnd, int x, int y, AppState& app)
{
    UNREFERENCED_PARAMETER(hWnd);

    if (PointInRectEx(app.signUpButton.rect, x, y) && app.signUpButton.enabled)
    {
        std::string playerName = ReadWindowTextA(app.hNameEdit);
        std::string password = ReadWindowTextA(app.hPasswordEdit);

        app.startMenu.SetPlayerName(playerName);
        app.startMenu.SetPassword(password);
        app.startMenu.SignUp(app.bridge);

        app.pendingRoomRequest = true;
        app.statusText = "Signing up...";
        return;
    }

    if (PointInRectEx(app.signInButton.rect, x, y) && app.signInButton.enabled)
    {
        std::string playerName = ReadWindowTextA(app.hNameEdit);
        std::string password = ReadWindowTextA(app.hPasswordEdit);

        app.startMenu.SetPlayerName(playerName);
        app.startMenu.SetPassword(password);
        app.startMenu.SignIn(app.bridge);

        app.pendingRoomRequest = true;
        app.statusText = "Logging in...";
        return;
    }

    if (app.pendingRoomRequest)
    {
        return;
    }

    if (PointInRectEx(app.createButton.rect, x, y) && app.createButton.enabled)
    {
        int playerCount = ReadComboSelectedInt(app.hPlayerCountCombo, RuleBook::MIN_PLAYERS);
        int visIndex = (int)SendMessageW(app.hVisibilityCombo, CB_GETCURSEL, 0, 0);

        app.startMenu.SetDesiredPlayerCount(playerCount);
        app.startMenu.SetRoomVisibility(visIndex == 1 ? RoomVisibility::Public : RoomVisibility::Private);
        app.startMenu.CreateRoom(app.viewport, app.bridge);

        app.pendingRoomRequest = true;
        app.statusText = "Creating room...";
        return;
    }

    if (PointInRectEx(app.joinButton.rect, x, y) && app.joinButton.enabled)
    {
        int roomId = ReadEditInt(app.hRoomEdit, 0);
        app.startMenu.SetTargetRoomId(roomId);
        app.startMenu.JoinRoom(app.viewport, app.bridge);

        app.pendingRoomRequest = true;
        app.statusText = "Joining room...";
        return;
    }

    for (std::size_t i = 0; i < app.publicRoomJoinRects.size() && i < app.publicRooms.size(); ++i)
    {
        if (PointInRectEx(app.publicRoomJoinRects[i], x, y))
        {
            app.startMenu.SetTargetRoomId(app.publicRooms[i].roomId);
            app.startMenu.JoinRoom(app.viewport, app.bridge);
            app.pendingRoomRequest = true;
            app.statusText = "Joining public room...";
            return;
        }
    }

    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    if (PointInRectEx(app.historyButton.rect, x, y) && app.historyButton.enabled)
    {
        app.bridge.SendHistoryRequest();
        app.statusText = "Loading history...";
        return;
    }
}

static bool HandleSocialPopupClick(int x, int y, AppState& app, const RECT& clientRect)
{
    if (!app.authenticated || !app.socialPopupOpen)
    {
        return false;
    }

    RECT popup = GetSocialPopupRect(clientRect);

    if (app.socialChatViewOpen)
    {
        if (PointInRectEx(GetSocialBackButtonRect(popup), x, y))
        {
            app.socialChatViewOpen = false;
            app.selectedFriendName.clear();
            app.searchedUsername.clear();
            SetWindowTextW(app.hSocialSearchEdit, L"");
            SetWindowTextW(app.hSocialPrivateChatEdit, L"");
            app.socialStatusText = "Back to friends.";
            return true;
        }

        if (PointInRectEx(GetSocialPrivateSendRect(popup), x, y))
        {
            std::string text = ReadWindowTextA(app.hSocialPrivateChatEdit);
            if (!text.empty() && !app.selectedFriendName.empty())
            {
                app.bridge.SendPrivateMessage(app.selectedFriendName, text);
                SetWindowTextW(app.hSocialPrivateChatEdit, L"");
            }
            return true;
        }

        return false;
    }

    for (std::size_t i = 0; i < app.pendingRequestRects.size() && i < app.pendingFriendRequests.size(); ++i)
    {
        if (PointInRectEx(app.pendingRequestRects[i], x, y))
        {
            app.bridge.SendFriendRequestResponse(app.pendingFriendRequests[i], true);
            app.socialStatusText = "Accepting friend request...";
            return true;
        }
    }

    for (std::size_t i = 0; i < app.friendRects.size() && i < app.friendsList.size(); ++i)
    {
        if (PointInRectEx(app.friendRects[i], x, y))
        {
            app.selectedFriendName = app.friendsList[i];
            app.socialChatViewOpen = true;
            app.socialStatusText = "Opened chat with " + app.selectedFriendName;
            return true;
        }
    }

    if (PointInRectEx(GetSocialSearchButtonRect(popup), x, y))
    {
        std::string username = ReadWindowTextA(app.hSocialSearchEdit);
        if (!username.empty())
        {
            app.bridge.SendSearchUserRequest(username);
            app.socialStatusText = "Searching...";
        }
        return true;
    }

    if (PointInRectEx(GetSocialAddFriendButtonRect(popup), x, y))
    {
        if (!app.searchedUsername.empty())
        {
            app.bridge.SendFriendRequest(app.searchedUsername);
            app.socialStatusText = "Sending friend request...";
        }
        else
        {
            std::string username = ReadWindowTextA(app.hSocialSearchEdit);
            if (!username.empty())
            {
                app.socialStatusText = "Search the username first.";
            }
            else
            {
                app.socialStatusText = "Enter a username first.";
            }
        }

        return true;
    }

    return false;
}

static void HandleWaitingRoomClick(int x, int y, AppState& app)
{
    if (PointInRectEx(app.readyButton.rect, x, y))
    {
        app.waitingRoom.ToggleReady(app.bridge);
        app.statusText = "Toggled ready.";
        return;
    }

    if (PointInRectEx(app.quitButton.rect, x, y))
    {
        app.bridge.SendLeaveRoomRequest();
        app.statusText = "Leaving room...";
        return;
    }
}

static RECT GetResponsiveBoardRect(const RECT& clientRect)
{
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int margin = 40;
    int topOffset = 120;
    int sidebarWidth = 220;

    // reserve space below the board for digit buttons + gap + bottom padding
    int digitButtonHeight = 40;
    int digitButtonGap = 20;
    int bottomMargin = 40;
    int reservedBottom = digitButtonGap + digitButtonHeight + bottomMargin;

    int availableW = clientW - margin * 2 - sidebarWidth;
    int availableH = clientH - topOffset - reservedBottom;

    int boardSize = (availableW < availableH) ? availableW : availableH;
    if (boardSize < 180)
    {
        boardSize = 180;
    }

    RECT boardRect
    {
        margin,
        topOffset,
        margin + boardSize,
        topOffset + boardSize
    };

    return boardRect;
}

static RECT GetSidebarRect(const RECT& clientRect)
{
    RECT boardRect = GetResponsiveBoardRect(clientRect);

    int left = boardRect.right + 30;
    int top = boardRect.top;
    int preferredWidth = 260;
    int right = left + preferredWidth;

    if (right > clientRect.right - 30)
    {
        right = clientRect.right - 30;
    }

    int bottom = boardRect.bottom;

    if (right < left + 140)
    {
        right = left + 140;
    }

    RECT sidebarRect{ left, top, right, bottom };
    return sidebarRect;
}

static RECT GetNotesButtonRect(const RECT& clientRect)
{
    RECT sidebarRect = GetSidebarRect(clientRect);

    RECT rect
    {
        sidebarRect.left,
        sidebarRect.top,
        sidebarRect.right,
        sidebarRect.top + 36
    };

    return rect;
}

static RECT GetHintButtonRect(const RECT& clientRect)
{
    RECT sidebarRect = GetSidebarRect(clientRect);

    RECT rect
    {
        sidebarRect.left,
        sidebarRect.top + 46,
        sidebarRect.right,
        sidebarRect.top + 82
    };

    return rect;
}

static RECT GetChatPanelRect(const RECT& clientRect)
{
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    if (gApp.viewport.GetPhase() == ScreenPhase::Playing)
    {
        RECT sidebar = GetSidebarRect(clientRect);

        RECT rect
        {
            sidebar.left,
            sidebar.top + 220,
            sidebar.right,
            clientRect.bottom - 30
        };

        return rect;
    }

    // Waiting room layout
    int panelW = max(260, clientW / 3);
    int left = clientRect.right - panelW - 30;
    int top = 70;
    int bottom = clientRect.bottom - 30;

    RECT rect
    {
        left,
        top,
        clientRect.right - 30,
        bottom
    };

    return rect;
}

static RECT GetChatInputRect(const RECT& clientRect)
{
    RECT panel = GetChatPanelRect(clientRect);

    RECT rect
    {
        panel.left,
        panel.bottom - 34,
        panel.right - 90,
        panel.bottom
    };

    return rect;
}

static RECT GetChatSendButtonRect(const RECT& clientRect)
{
    RECT panel = GetChatPanelRect(clientRect);

    RECT rect
    {
        panel.right - 80,
        panel.bottom - 34,
        panel.right,
        panel.bottom
    };

    return rect;
}

static RECT GetHistoryBackButtonRect(const RECT& clientRect)
{
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int marginX = max(20, clientW / 18);
    int titleTop = max(18, clientH / 30);
    int titleHeight = max(35, clientH / 14);

    int btnW = max(70, clientW / 14);
    int btnH = max(28, clientH / 22);

    RECT rect
    {
        marginX,
        titleTop + titleHeight + 6,
        marginX + btnW,
        titleTop + titleHeight + 6 + btnH
    };

    return rect;
}

static RECT GetSocialLauncherRect(const RECT& clientRect)
{
    int size = 58;
    return RECT{ clientRect.right - 85, clientRect.bottom - 85, clientRect.right - 27, clientRect.bottom - 27 };
}

static RECT GetSocialPopupRect(const RECT& clientRect)
{
    return RECT{ clientRect.right - 360, 120, clientRect.right - 30, clientRect.bottom - 110 };
}

static RECT GetSocialSearchRect(const RECT& popupRect)
{
    return RECT{ popupRect.left + 16, popupRect.top + 46, popupRect.right - 120, popupRect.top + 78 };
}

static RECT GetSocialSearchButtonRect(const RECT& popupRect)
{
    return RECT{ popupRect.right - 96, popupRect.top + 46, popupRect.right - 16, popupRect.top + 78 };
}

static RECT GetSocialAddFriendButtonRect(const RECT& popupRect)
{
    return RECT{ popupRect.left + 16, popupRect.top + 88, popupRect.right - 16, popupRect.top + 122 };
}

static RECT GetSocialPrivateInputRect(const RECT& popupRect)
{
    return RECT{ popupRect.left + 16, popupRect.bottom - 42, popupRect.right - 90, popupRect.bottom - 10 };
}

static RECT GetSocialPrivateSendRect(const RECT& popupRect)
{
    return RECT{ popupRect.right - 82, popupRect.bottom - 42, popupRect.right - 16, popupRect.bottom - 10 };
}

static RECT GetSocialBackButtonRect(const RECT& popupRect)
{
    return RECT{ popupRect.right - 96, popupRect.top + 10, popupRect.right - 16, popupRect.top + 42 };
}

static RECT GetSocialChatMessagesRect(const RECT& popupRect)
{
    return RECT{ popupRect.left + 16, popupRect.top + 56, popupRect.right - 16, popupRect.bottom - 70 };
}

static RECT GetHistoryContentRect(const RECT& clientRect)
{
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    int marginX = max(20, clientW / 18);
    int topY = max(120, clientH / 5);
    int rightX = clientRect.right - marginX;
    int bottomY = clientRect.bottom - max(20, clientH / 20);

    RECT rect{ marginX, topY, rightX, bottomY };
    return rect;
}
static RECT GetHistoryHeaderRect(const RECT& contentRect, int index, int totalColumns)
{
    UNREFERENCED_PARAMETER(totalColumns);

    int width = contentRect.right - contentRect.left;

    int colWidths[7];
    colWidths[0] = width * 8 / 100;   // icon
    colWidths[1] = width * 24 / 100;  // date/time
    colWidths[2] = width * 12 / 100;  // score
    colWidths[3] = width * 12 / 100;  // wrong
    colWidths[4] = width * 10 / 100;  // hints
    colWidths[5] = width * 20 / 100;  // time/avg
    colWidths[6] = width - (colWidths[0] + colWidths[1] + colWidths[2] + colWidths[3] + colWidths[4] + colWidths[5]); // rank

    int left = contentRect.left;
    for (int i = 0; i < index; ++i)
    {
        left += colWidths[i];
    }

    RECT rect
    {
        left,
        contentRect.top,
        left + colWidths[index],
        contentRect.top + 36
    };

    return rect;
}

static RECT GetHistoryCellRect(const RECT& contentRect, int rowTop, int rowHeight, int index, int totalColumns)
{
    UNREFERENCED_PARAMETER(totalColumns);

    int width = contentRect.right - contentRect.left;

    int colWidths[7];
    colWidths[0] = width * 8 / 100;
    colWidths[1] = width * 24 / 100;
    colWidths[2] = width * 12 / 100;
    colWidths[3] = width * 12 / 100;
    colWidths[4] = width * 10 / 100;
    colWidths[5] = width * 20 / 100;
    colWidths[6] = width - (colWidths[0] + colWidths[1] + colWidths[2] + colWidths[3] + colWidths[4] + colWidths[5]);

    int left = contentRect.left;
    for (int i = 0; i < index; ++i)
    {
        left += colWidths[i];
    }

    RECT rect
    {
        left,
        rowTop,
        left + colWidths[index],
        rowTop + rowHeight
    };

    return rect;
}
static bool GetSelectedCell(const LocalBoardState& board, int& outRow, int& outCol)
{
    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            if (board.cells[row][col].selected)
            {
                outRow = row;
                outCol = col;
                return true;
            }
        }
    }

    return false;
}

static void HandlePuzzleClick(int x, int y, AppState& app, const RECT& clientRect)
{
    RECT boardRect = GetResponsiveBoardRect(clientRect);

    if (!PointInRectEx(boardRect, x, y))
    {
        return;
    }

    int boardW = boardRect.right - boardRect.left;
    int boardH = boardRect.bottom - boardRect.top;

    int localX = x - boardRect.left;
    int localY = y - boardRect.top;

    int col = (localX * RuleBook::BOARD_SIZE) / boardW;
    int row = (localY * RuleBook::BOARD_SIZE) / boardH;

    if (col < 0) col = 0;
    if (col >= RuleBook::BOARD_SIZE) col = RuleBook::BOARD_SIZE - 1;
    if (row < 0) row = 0;
    if (row >= RuleBook::BOARD_SIZE) row = RuleBook::BOARD_SIZE - 1;

    app.puzzleArena.SelectCell(app.viewport, row, col);
}

static void HandlePuzzleKey(WPARAM wParam, AppState& app)
{
    int row = -1;
    int col = -1;

    if (!GetSelectedCell(app.viewport.GetBoard(), row, col))
    {
        return;
    }

    if (wParam >= '1' && wParam <= '9')
    {
        int value = (int)(wParam - '0');
        app.puzzleArena.SubmitDigit(app.viewport, app.bridge, row, col, value);
        return;
    }

    if (wParam == VK_BACK || wParam == VK_DELETE)
    {
        app.puzzleArena.EraseCell(app.viewport, row, col);
        return;
    }

    if (wParam == 'N')
    {
        app.puzzleArena.ToggleNotes(app.viewport);
        return;
    }

    if (wParam == 'H')
    {
        app.puzzleArena.UseHint(app.viewport, app.bridge);
        return;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateChildControls(hWnd);
        UpdateStartMenuLayout(hWnd, gApp);

        InitGdiplusIfNeeded();
        EnsureStartMenuBackgroundLoaded();

        gApp.hWinCrownIcon = (HICON)LoadImageW(
            nullptr,
            L"wincrown.ico",
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE
        );

        gApp.hLoseCrownIcon = (HICON)LoadImageW(
            nullptr,
            L"fallcrown.ico",
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE
        );

        gApp.connected = gApp.bridge.Connect(54000);
        gApp.statusText = gApp.connected ? "Searching for LAN server..." : "Failed to initialize UDP client.";

        gApp.lastInputTick = GetTickCount64();
        gApp.lastHeartbeatTick = 0;

        StartBackgroundMusic();

        SetTimer(hWnd, TIMER_ID_MAIN, TIMER_INTERVAL_MS, nullptr);
        UpdateControlVisibility(gApp);
        SetFocus(hWnd);
    }
    return 0;

    case WM_TIMER:
    {
        ScreenPhase oldPhase = gApp.viewport.GetPhase();
        std::string oldStatus = gApp.statusText;
        std::string oldSocialStatus = gApp.socialStatusText;
        std::string oldSearchedUsername = gApp.searchedUsername;
        std::size_t oldPublicRoomCount = gApp.publicRooms.size();

        gApp.bridge.PollIncoming();
        gApp.bridge.PollResends();
        ProcessIncomingMessages(gApp);
        UpdateControlVisibility(gApp);
        UpdateIdleState(hWnd, gApp);

        static int publicRoomRefreshTick = 0;

        if (gApp.viewport.GetPhase() == ScreenPhase::StartMenu)
        {
            ++publicRoomRefreshTick;
            if (publicRoomRefreshTick >= 60)
            {
                gApp.bridge.SendRequestPublicRoomList();
                publicRoomRefreshTick = 0;
            }
        }
        else
        {
            publicRoomRefreshTick = 0;
        }

        bool publicRoomsChanged = (gApp.publicRooms.size() != oldPublicRoomCount);
        bool socialStatusChanged = (gApp.socialStatusText != oldSocialStatus);
        bool searchedUserChanged = (gApp.searchedUsername != oldSearchedUsername);

        if (gApp.viewport.GetPhase() != oldPhase ||
            gApp.statusText != oldStatus ||
            socialStatusChanged ||
            searchedUserChanged ||
            gApp.viewport.GetPhase() != ScreenPhase::StartMenu ||
            publicRoomsChanged)
        {
            InvalidateRect(hWnd, nullptr, FALSE);
        }
    }
    return 0;

    case WM_MOUSEMOVE:
    {
        if (!gApp.localTimedOut)
        {
            MarkUserActivity(gApp);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {


        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        MarkUserActivity(gApp);

        RECT clientRect{};
        GetClientRect(hWnd, &clientRect);

        if (gApp.authenticated &&
            (gApp.viewport.GetPhase() == ScreenPhase::StartMenu ||
                gApp.viewport.GetPhase() == ScreenPhase::WaitingRoom ||
                gApp.viewport.GetPhase() == ScreenPhase::Playing))
        {
            RECT launcherRect = GetSocialLauncherRect(clientRect);
            if (PointInRectEx(launcherRect, x, y))
            {
                gApp.socialPopupOpen = !gApp.socialPopupOpen;
                if (!gApp.socialPopupOpen)
                {
                    gApp.socialChatViewOpen = false;
                    gApp.selectedFriendName.clear();
                    gApp.searchedUsername.clear();
                    SetWindowTextW(gApp.hSocialSearchEdit, L"");
                    SetWindowTextW(gApp.hSocialPrivateChatEdit, L"");
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }

        if (HandleSocialPopupClick(x, y, gApp, clientRect))
        {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        switch (gApp.viewport.GetPhase())
        {

        case ScreenPhase::StartMenu:
            HandleStartMenuClick(hWnd, x, y, gApp);
            InvalidateRect(hWnd, nullptr, FALSE);
            break;

        case ScreenPhase::WaitingRoom:
        {
            HandleWaitingRoomClick(x, y, gApp);

            RECT clientRect{};
            GetClientRect(hWnd, &clientRect);
            RECT sendRect = GetChatSendButtonRect(clientRect);

            if (PointInRectEx(sendRect, x, y))
            {
                std::string text = ReadWindowTextA(gApp.hChatEdit);
                if (!text.empty())
                {
                    gApp.bridge.SendRoomChatMessage(text);
                    SetWindowTextW(gApp.hChatEdit, L"");
                }
            }

            break;
        }

        case ScreenPhase::Playing:
        {
            RECT clientRect{};
            GetClientRect(hWnd, &clientRect);

            if (gApp.presencePopupVisible)
            {
                RECT popup = GetPresencePopupRect(clientRect);

                if (gApp.localTimedOut)
                {
                    RECT resumeRect = GetPresenceResumeButtonRect(popup);

                    if (PointInRectEx(resumeRect, x, y))
                    {
                        gApp.statusText = "Requesting resume...";
                        gApp.bridge.SendResumeMatchRequest();
                    }

                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                else
                {
                    RECT yesRect = GetPresenceYesButtonRect(popup);

                    if (PointInRectEx(yesRect, x, y))
                    {
                        gApp.localTimedOut = false;
                        gApp.presencePopupVisible = false;
                        gApp.lastInputTick = GetTickCount64();
                        gApp.bridge.SendHeartbeat();
                        gApp.statusText = "Continuing current match.";
                    }

                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            if (gApp.localTimedOut)
            {
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            RECT notesRect = GetNotesButtonRect(clientRect);
            RECT hintRect = GetHintButtonRect(clientRect);

            if (PointInRectEx(notesRect, x, y))
            {
                gApp.puzzleArena.ToggleNotes(gApp.viewport);
            }
            else if (PointInRectEx(hintRect, x, y) &&
                gApp.viewport.GetHintState().unlocked &&
                gApp.viewport.GetHintState().availableCount > 0)
            {
                gApp.puzzleArena.UseHint(gApp.viewport, gApp.bridge);
            }
            else
            {
                bool handled = false;

                // digit buttons first
                for (int i = 0; i < 9; ++i)
                {
                    if (PointInRectEx(gApp.digitButtons[i].rect, x, y))
                    {
                        int row, col;
                        if (GetSelectedCell(gApp.viewport.GetBoard(), row, col))
                        {
                            if (gApp.digitButtons[i].enabled)
                            {
                                gApp.puzzleArena.SubmitDigit(
                                    gApp.viewport,
                                    gApp.bridge,
                                    row,
                                    col,
                                    i + 1
                                );
                            }
                        }
                        handled = true;
                        break;
                    }
                }

                if (!handled)
                {
                    HandlePuzzleClick(x, y, gApp, clientRect);
                }
            }

            RECT sendRect = GetChatSendButtonRect(clientRect);
            if (PointInRectEx(sendRect, x, y))
            {
                std::string text = ReadWindowTextA(gApp.hChatEdit);
                if (!text.empty())
                {
                    gApp.bridge.SendRoomChatMessage(text);
                    SetWindowTextW(gApp.hChatEdit, L"");
                }
            }

            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }

        case ScreenPhase::RoundSummary:
        {
            if (PointInRectEx(gApp.roundSummaryHomeButton.rect, x, y))
            {
                gApp.bridge.SendLeaveRoomRequest();
                gApp.statusText = "Returning to main menu...";
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }

        case ScreenPhase::History:
        {
            RECT clientRect{};
            GetClientRect(hWnd, &clientRect);
            gApp.backButton.rect = GetHistoryBackButtonRect(clientRect);

            if (PointInRectEx(gApp.backButton.rect, x, y))
            {
                gApp.viewport.SetPhase(ScreenPhase::StartMenu);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }

        default:
            break;
        }
    }
    return 0;

    case WM_KEYDOWN:
    {
        if (gApp.viewport.GetPhase() == ScreenPhase::Playing)
        {
            if (!gApp.localTimedOut)
            {
                MarkUserActivity(gApp);
                HandlePuzzleKey(wParam, gApp);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        else if (gApp.viewport.GetPhase() == ScreenPhase::RoundSummary)
        {
            if (wParam == 'P')
            {
                gApp.roundSummary.TogglePlayAgain(gApp.bridge);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
    }
    return 0;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_SIZE:
    {
        if (gApp.viewport.GetPhase() == ScreenPhase::StartMenu)
        {
            UpdateStartMenuLayout(hWnd, gApp);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
    }
    return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRect{};
        GetClientRect(hWnd, &clientRect);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(
            hdc,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top);

        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        if (gApp.viewport.GetPhase() != ScreenPhase::StartMenu)
        {
            HBRUSH bg = CreateSolidBrush(RGB(245, 245, 245));
            FillRect(memDC, &clientRect, bg);
            DeleteObject(bg);
        }

        DrawCurrentScreen(memDC, clientRect, gApp);

        BitBlt(
            hdc,
            0, 0,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top,
            memDC,
            0, 0,
            SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
    }
    return 0;

    case WM_CLOSE:
    {
        if (gApp.bridge.IsConnected() && gApp.bridge.GetAssignedRoomId() != 0)
        {
            gApp.bridge.SendLeaveRoomRequest();

            DWORD start = GetTickCount();
            while (GetTickCount() - start < 200)
            {
                gApp.bridge.PollIncoming();
                gApp.bridge.PollResends();
                Sleep(10);
            }
        }

        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID_MAIN);

        if (gApp.hWinCrownIcon != nullptr)
        {
            DestroyIcon(gApp.hWinCrownIcon);
            gApp.hWinCrownIcon = nullptr;
        }

        if (gApp.hLoseCrownIcon != nullptr)
        {
            DestroyIcon(gApp.hLoseCrownIcon);
            gApp.hLoseCrownIcon = nullptr;
        }

        StopBackgroundMusic();
        ShutdownGdiplus();
        gApp.bridge.Disconnect();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }

    return (INT_PTR)FALSE;
}