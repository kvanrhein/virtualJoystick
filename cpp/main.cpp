#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>

#include <list>
#include <memory>
using namespace std;

#pragma comment(lib, "d2d1")
#pragma comment(lib, "Dwrite")

#include "stdafx.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <dwrite.h>
#include "basewin.h"
#include "resource.h"
#include "public.h"
#include "vjoyinterface.h"
#define _USE_MATH_DEFINES 
#include <cmath>
#define DEV_ID      1
#define BUTTON_CNT 12
#define PI 3.1416

float distancePoints(float x1, float y1, float x2, float y2) {
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

float calcAngle(float x1, float y1, float x2, float y2, float x3, float y3) {
    return - atan2f(y3 - y1, x3 - x1) + atan2(y2 - y1, x2 - x1);
}

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class DPIScale
{
    static float scaleX;
    static float scaleY;

public:
    static void Initialize(ID2D1Factory* pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX / 96.0f;
        scaleY = dpiY / 96.0f;
    }

    template <typename T>
    static float PixelsToDipsX(T x)
    {
        return static_cast<float>(x) / scaleX;
    }

    template <typename T>
    static float PixelsToDipsY(T y)
    {
        return static_cast<float>(y) / scaleY;
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct JoyStick {
    float x;
    float y;
    float twist;
    float radius;

    void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush) {
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_ELLIPSE joyStickEllipse;
        joyStickEllipse.point.x = x;
        joyStickEllipse.point.y = y;
        joyStickEllipse.radiusX = radius;
        joyStickEllipse.radiusY = radius;
        pRT->DrawEllipse(joyStickEllipse, pBrush);
        double pi = 3.1415926535;
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRT->DrawLine(
            D2D1::Point2F(x + radius * cos(twist), y-radius*sin(twist)),
            D2D1::Point2F(x + (radius + 40) * cos(twist), y-(radius + 40) * sin(twist)),
            pBrush,
            2.5f
        );
    }
};

struct JoyButton {
    D2D1_POINT_2F pos;
    D2D1_SIZE_F size;
    boolean pressed;
    int number;

    void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush, D2D1_SIZE_U winSize, IDWriteTextFormat* tf) {
        WCHAR label[4] = L"";
        swprintf_s(label, L"%d ", number);

        size.height = ((winSize.height - 20) / BUTTON_CNT) - 5;
        size.width = size.height;
        pos.x = winSize.width - size.width - 5;
        pos.y = (number - 1) * (size.height + 5) + 10;
        D2D1_RECT_F rect = D2D1::RectF(pos.x, pos.y, pos.x + size.width, pos.y + size.height);
        pBrush->SetColor(D2D1::ColorF(pressed ? D2D1::ColorF::Red : D2D1::ColorF::Green));
        pRT->DrawRectangle(&rect, pBrush, pressed ? 4 : 2);
        pRT->DrawText(label, ARRAYSIZE(label) - 1, tf, rect, pBrush);
    }

    void mouseUp() {
        pressed = false;
    }

    void mouseDown(int x, int y) {
        if (x > pos.x && x < pos.x + size.width && y > pos.y && y < pos.y + size.height) {
            pressed = true;
        }
    }
};


D2D1::ColorF::Enum colors[] = { D2D1::ColorF::Yellow, D2D1::ColorF::Salmon, D2D1::ColorF::LimeGreen };


class MainWindow : public BaseWindow<MainWindow>
{
    enum Mode
    {
        DrawMode,
        SelectMode,
        DragMode,
        TwistMode,
    };

    HCURSOR                 hCursor;
    D2D1_SIZE_U             size;
    UINT32                  targetSize;
    UINT32                  midPoint;
    ID2D1Factory* pFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    ID2D1SolidColorBrush* pBrush;
    D2D1_POINT_2F           ptMouse;
    D2D1_ELLIPSE            targetOutline;
    JoyStick                theJoyStick;
    JoyButton               buttons[BUTTON_CNT];
    Mode                    mode;
    size_t                  nextColor;
    boolean                 stillTwisting;

    IDWriteFactory* writeFactory;
    ID2D1Factory* pD2DFactory;
    IDWriteTextFormat* textFormat;

    BOOL    HitTest(float x, float y);
    void    SetMode(Mode m);
    void    MoveSelection(float x, float y);
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    Resize();
    void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void    OnLButtonUp();
    void    OnMouseMove(int pixelX, int pixelY, DWORD flags);
    void    OnKeyDown(UINT vkey);
    void    OnKeyUp(UINT vkey);

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL),
        ptMouse(D2D1::Point2F()), nextColor(0) 
    {
        theJoyStick.radius = 20;
        theJoyStick.twist = 0;
        stillTwisting = false;

        HRESULT hr;
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
        if (SUCCEEDED(hr))
        {
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&writeFactory)
            );
        }

        if (SUCCEEDED(hr))
        {
            hr = writeFactory->CreateTextFormat(
                L"Arial",                // Font family name.
                NULL,                       // Font collection (NULL sets it to use the system font collection).
                DWRITE_FONT_WEIGHT_REGULAR,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f,
                L"en-us",
                &textFormat
            );
        }

        if (SUCCEEDED(hr))
        {
            hr = textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        if (SUCCEEDED(hr))
        {
            hr = textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }


        UINT DevID = DEV_ID;
        if (!vJoyEnabled())
        {
            //that's bad
            wprintf(L"can not enable joystick");
        }
        else
        {
            wprintf(L"Vendor: %s\nProduct :%s\nVersion Number:%s\n", static_cast<TCHAR*> (GetvJoyManufacturerString()), static_cast<TCHAR*>(GetvJoyProductString()), static_cast<TCHAR*>(GetvJoySerialNumberString()));
        };
        VjdStat status = GetVJDStatus(DEV_ID);

        switch (status)
        {
        case VJD_STAT_OWN:
            _tprintf(L"vJoy device %d is already owned by this feeder\n", DevID);
            break;
        case VJD_STAT_FREE:
            _tprintf(L"vJoy device %d is free\n", DevID);
            break;
        case VJD_STAT_BUSY:
            _tprintf(L"vJoy device %d is already owned by another feeder\nCannot continue\n", DevID);
            break;
        case VJD_STAT_MISS:
            _tprintf(L"vJoy device %d is not installed or disabled\nCannot continue\n", DevID);
            break;
        default:
            _tprintf(L"vJoy device %d general error\nCannot continue\n", DevID);
            break;
        };

        if (!AcquireVJD(DevID))
        {
            _tprintf(L"Failed to acquire vJoy device number %d.\n", DevID);
        }
        else
            _tprintf(L"Acquired device number %d - OK\n", DevID);

        ResetVJD(1);
    }

    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        size = D2D1::SizeU(rc.right, rc.bottom);
        theJoyStick.x = midPoint;
        theJoyStick.y = midPoint;

        for (int i = 0; i < BUTTON_CNT; i++) {
            buttons[i].pos.x = size.width - 30;
            buttons[i].pos.y = i * 30 + 10;
            buttons[i].number = i + 1;
            buttons[i].pressed = false;
        }

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
        }
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

void MainWindow::OnPaint()
{
    int rsizeX = 0;
    int rsizeY = 0;
    static boolean firstTime = true;

    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        //Target
        targetSize = (UINT32)min(size.width, size.height) - 50.0f;
        midPoint = targetSize / 2.0f + 25.0f;
        targetOutline.point.x = midPoint;
        targetOutline.point.y = midPoint;
        targetOutline.radiusX = midPoint - 25.0f;
        targetOutline.radiusY = midPoint - 25.0f;
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
        pRenderTarget->DrawEllipse(targetOutline, pBrush);
        pRenderTarget->DrawLine(
            D2D1::Point2F(25.0f, midPoint),
            D2D1::Point2F(25.0f + targetSize, midPoint),
            pBrush,
            2.5f
        );

        pRenderTarget->DrawLine(
            D2D1::Point2F(midPoint, 25.0f),
            D2D1::Point2F(midPoint, 25.0f + targetSize),
            pBrush,
            1.0f
        );

        if (firstTime) {
            theJoyStick.x = midPoint;
            theJoyStick.y = midPoint;
            firstTime = false;
        }

        theJoyStick.Draw(pRenderTarget, pBrush);

        for (int i = 0; i < 12; i++)
            buttons[i].Draw(pRenderTarget, pBrush, size, textFormat);

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }

    //Send calls to vJoy, from vJoyInterface.dll:
    int xAxis = (theJoyStick.x - midPoint) / (targetSize / 2) * 16000 + 16000;
    int yAxis = -(theJoyStick.y - midPoint) / (targetSize / 2) * 16000 + 16000;
    int zAxis = theJoyStick.twist * 16000 / 3.14 * 2 + 16000;
    SetAxis(xAxis, 1, HID_USAGE_X);
    SetAxis(yAxis, 1, HID_USAGE_Y);
    SetAxis(zAxis, 1, HID_USAGE_Z);
    for (int i = 0; i < BUTTON_CNT; i++) {
        SetBtn(buttons[i].pressed, 1, i + 1);
    }
    //end of vJoy section

}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        size = D2D1::SizeU(rc.right, rc.bottom);
        pRenderTarget->Resize(size);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
    theJoyStick.x = midPoint;
    theJoyStick.y = midPoint;

}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    for (int i = 0; i < BUTTON_CNT; i++) {
        buttons[i].mouseDown(dipX, dipY);
    }


    float d = distancePoints(targetOutline.point.x, targetOutline.point.y, dipX, dipY);
    if (d > targetOutline.radiusX - 5 && d < targetOutline.radiusX + 5) {
        SetMode(TwistMode);
        stillTwisting = true;
    }

    if (mode == DrawMode || mode == TwistMode)
    {
        POINT pt = { pixelX, pixelY };
        if (DragDetect(m_hwnd, pt))
        {
            SetCapture(m_hwnd);
            ptMouse = D2D1::Point2F(dipX, dipY);
        }
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonUp()
{
    theJoyStick.x = midPoint;
    theJoyStick.y = midPoint;
    theJoyStick.twist = 0;

    for (int i = 0; i < BUTTON_CNT; i++) {
        buttons[i].mouseUp();
    }

    stillTwisting = false;
    SetMode(DrawMode);
    InvalidateRect(m_hwnd, NULL, FALSE);
    ReleaseCapture();
}


void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    float d = distancePoints(targetOutline.point.x, targetOutline.point.y, dipX, dipY);
    if (d > targetOutline.radiusX - 5 && d < targetOutline.radiusX + 5) {
        SetMode(TwistMode);
    }
    else if (!stillTwisting) {
        SetMode(DrawMode);
    }
        


    if ((flags & MK_LBUTTON))
    {
        float diffx = 0;
        float diffy = 0;
        if (mode == DrawMode || mode == TwistMode)
        {
            diffx = (dipX - ptMouse.x);
            diffy = (dipY - ptMouse.y);
        }
        if (mode == DrawMode) {
            theJoyStick.x = midPoint + diffx;
            theJoyStick.y = midPoint + diffy;
        }
        if (mode == TwistMode) {
            theJoyStick.twist = calcAngle(targetOutline.point.x, targetOutline.point.y, ptMouse.x, ptMouse.y, dipX, dipY);
            if (theJoyStick.twist > PI)
                theJoyStick.twist -= PI * 2;
            if (theJoyStick.twist < -PI)
                theJoyStick.twist += PI * 2;
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnKeyUp(UINT vkey)
{
    if (vkey > 0x30 && vkey < 0x30 + BUTTON_CNT) {
        buttons[vkey - 0x30 - 1].pressed = false;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnKeyDown(UINT vkey)
{
    if (vkey > 0x30 && vkey < 0x30 + BUTTON_CNT) {
        buttons[vkey - 0x30 - 1].pressed = true;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}



BOOL MainWindow::HitTest(float x, float y)
{
    return FALSE;
}


void MainWindow::SetMode(Mode m)
{
    mode = m;

    LPWSTR cursor;
    switch (mode)
    {
    case TwistMode:
        cursor = IDC_HAND;
        break;
    case DrawMode:
        cursor = IDC_CROSS;
        break;

    case SelectMode:
        cursor = IDC_HAND;
        break;

    case DragMode:
        cursor = IDC_SIZEALL;
        break;
    }

    hCursor = LoadCursor(NULL, cursor);
    SetCursor(hCursor);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Virtual Joystick 1781", WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));

    ShowWindow(win.Window(), nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(win.Window(), hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
        DPIScale::Initialize(pFactory);
        SetMode(DrawMode);
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_SIZE:
        Resize();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        SetTimer(m_hwnd, 1, 1000, (TIMERPROC)NULL);
        return 0;

    case WM_TIMER:
        for (int i = 0; i < BUTTON_CNT; i++) {
            buttons[i].mouseUp();
        }
        KillTimer(m_hwnd, 1);
        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp();
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(hCursor);
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        OnKeyDown((UINT)wParam);
        return 0;

    case WM_KEYUP:
        OnKeyUp((UINT)wParam);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_DRAW_MODE:
            SetMode(DrawMode);
            break;

        case ID_SELECT_MODE:
            SetMode(SelectMode);
            break;

        case ID_TOGGLE_MODE:
            if (mode == DrawMode)
            {
                SetMode(SelectMode);
            }
            else
            {
                SetMode(DrawMode);
            }
            break;
        }
        return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}



