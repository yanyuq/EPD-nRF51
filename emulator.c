// GUI emulator for Windows
// This code is a simple Windows GUI application that emulates the display of an e-paper device.
#include <windows.h>
#include <stdint.h>
#include <time.h>
#include "GUI.h"

#define BITMAP_WIDTH   400
#define BITMAP_HEIGHT  300
#define WINDOW_WIDTH   400
#define WINDOW_HEIGHT  340
#define WINDOW_TITLE   TEXT("Emurator")

// Global variables
HINSTANCE g_hInstance;
HWND g_hwnd;
display_mode_t g_display_mode = MODE_CALENDAR; // Default to calendar mode
BOOL g_bwr_mode = TRUE;  // Default to BWR mode
time_t g_display_time;
struct tm g_tm_time;

// Convert bitmap data from e-paper format to Windows DIB format
static uint8_t *convertBitmap(uint8_t *bitmap, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    int bytesPerRow = ((w + 31) / 32) * 4; // Round up to nearest 4 bytes
    int totalSize = bytesPerRow * h;
    
    // Allocate memory for converted bitmap
    uint8_t *convertedBitmap = (uint8_t*)malloc(totalSize);
    if (convertedBitmap == NULL) return NULL;
    
    memset(convertedBitmap, 0, totalSize);

    int ePaperBytesPerRow = (w + 7) / 8; // E-paper buffer stride
    
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            // Calculate byte and bit position in e-paper buffer
            int bytePos = row * ePaperBytesPerRow + col / 8;
            int bitPos = 7 - (col % 8); // MSB first (typical e-paper format)
            
            // Check if the bit is set in the e-paper buffer
            int isSet = (bitmap[bytePos] >> bitPos) & 0x01;
            
            // Calculate byte and bit position in Windows DIB
            int dibBytePos = row * bytesPerRow + col / 8;
            int dibBitPos = 7 - (col % 8); // MSB first for DIB too
            
            // Set the bit in the Windows DIB if it's set in the e-paper buffer
            if (isSet) {
                convertedBitmap[dibBytePos] |= (1 << dibBitPos);
            }
        }
    }
    
    return convertedBitmap;
}

// Implementation of the buffer_callback function
void DrawBitmap(uint8_t *black, uint8_t *color, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    HDC hdc;
    RECT clientRect;
    int scale = 1;
    
    // Get the device context for immediate drawing
    hdc = GetDC(g_hwnd);
    if (!hdc) return;
    
    // Get client area for positioning
    GetClientRect(g_hwnd, &clientRect);
    
    // Calculate position to center the entire bitmap in the window
    int drawX = (clientRect.right - BITMAP_WIDTH * scale) / 2;
    int drawY = (clientRect.bottom - BITMAP_HEIGHT * scale) / 2;
    
    // Create DIB for visible pixels
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Negative for top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    uint8_t *convertedBitmap = convertBitmap(black, x, y, w, h);
    if (convertedBitmap == NULL) {
        ReleaseDC(g_hwnd, hdc);
        return;
    }
    
    // Set colors for black and white display
    bmi.bmiColors[0].rgbBlue = 0;
    bmi.bmiColors[0].rgbGreen = 0;
    bmi.bmiColors[0].rgbRed = 0;
    bmi.bmiColors[0].rgbReserved = 0;
    
    bmi.bmiColors[1].rgbBlue = 255;
    bmi.bmiColors[1].rgbGreen = 255;
    bmi.bmiColors[1].rgbRed = 255;
    bmi.bmiColors[1].rgbReserved = 0;
    
    // Draw the black layer
    StretchDIBits(hdc,
                 drawX + x * scale, drawY + y * scale,  // Destination position
                 w * scale, h * scale,                  // Destination size
                 0, 0,                                 // Source position
                 w, h,                                 // Source size
                 convertedBitmap,                      // Converted bitmap bits
                 &bmi,                                 // Bitmap info
                 DIB_RGB_COLORS,                       // Usage
                 SRCCOPY);                             // Raster operation code
    free(convertedBitmap);

    // Handle color layer if present (red in BWR displays)
    if (color) {
        // Allocate memory for converted color bitmap
        uint8_t *convertedColor = convertBitmap(color, x, y, w, h);
        if (convertedColor) {
            // Set colors for red overlay
            bmi.bmiColors[0].rgbBlue = 255;
            bmi.bmiColors[0].rgbGreen = 255;
            bmi.bmiColors[0].rgbRed = 0;
            bmi.bmiColors[0].rgbReserved = 0;
            
            bmi.bmiColors[1].rgbBlue = 0;
            bmi.bmiColors[1].rgbGreen = 0;
            bmi.bmiColors[1].rgbRed = 0;
            bmi.bmiColors[1].rgbReserved = 0;
            
            // Draw red overlay
            StretchDIBits(hdc,
                         drawX + x * scale, drawY + y * scale,  // Destination position
                         w * scale, h * scale,                  // Destination size
                         0, 0,                                 // Source position
                         w, h,                                 // Source size
                         convertedColor,                       // Converted bitmap bits
                         &bmi,                                 // Bitmap info
                         DIB_RGB_COLORS,                       // Usage
                         SRCINVERT);                           // Use XOR operation to blend
                         
            free(convertedColor);
        }
    }
    
    // Release the device context
    ReleaseDC(g_hwnd, hdc);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            // Initialize the display time
            g_display_time = time(NULL) + 8*3600;
            // Set a timer to update the CLOCK periodically (every second)
            SetTimer(hwnd, 1, 1000, NULL);
            return 0;

        case WM_TIMER:
            if (g_display_mode == MODE_CLOCK) {
                g_display_time = time(NULL) + 8*3600;
                if (g_display_time % 60 == 0) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Get client rect for calculations
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            // Clear the entire client area with a solid color
            HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &clientRect, bgBrush);
            DeleteObject(bgBrush);
            
            // Use the stored timestamp
            gui_data_t data = {
                .bwr             = g_bwr_mode,
                .width           = BITMAP_WIDTH,
                .height          = BITMAP_HEIGHT,
                .timestamp       = g_display_time,
                .temperature     = 25,
                .voltage         = 3.2f,
            };
            
            // Call DrawGUI to render the interface, passing the BWR mode
            DrawGUI(&data, DrawBitmap, g_display_mode);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_KEYDOWN:
            // Toggle display mode with spacebar
            if (wParam == VK_SPACE) {
                if (g_display_mode == MODE_CLOCK)
                    g_display_mode = MODE_CALENDAR;
                else
                    g_display_mode = MODE_CLOCK;
                
                InvalidateRect(hwnd, NULL, TRUE);
            }
            // Toggle BWR mode with R key
            else if (wParam == 'R') {
                g_bwr_mode = !g_bwr_mode;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            // Handle arrow keys for month/day adjustment
            else if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT) {
                // Get the current time structure
                g_tm_time = *localtime(&g_display_time);
                
                // Up/Down adjusts month
                if (wParam == VK_UP) {
                    g_tm_time.tm_mon++;
                    if (g_tm_time.tm_mon > 11) {
                        g_tm_time.tm_mon = 0;
                        g_tm_time.tm_year++;
                    }
                }
                else if (wParam == VK_DOWN) {
                    g_tm_time.tm_mon--;
                    if (g_tm_time.tm_mon < 0) {
                        g_tm_time.tm_mon = 11;
                        g_tm_time.tm_year--;
                    }
                }
                // Left/Right adjusts day
                else if (wParam == VK_RIGHT) {
                    g_tm_time.tm_mday++;
                }
                else if (wParam == VK_LEFT) {
                    g_tm_time.tm_mday--;
                }
                
                // Convert back to time_t
                g_display_time = mktime(&g_tm_time);
                
                // Force redraw
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
            
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    
    // Register window class
    WNDCLASSA wc = {0}; // Using WNDCLASSA for ANSI version
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "BitmapDemo"; // No L prefix - using ANSI strings
    
    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // Create the window - explicit use of CreateWindowA for ANSI version
    g_hwnd = CreateWindowA(
        "BitmapDemo",
        "Emurator", // Using simple title
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwnd) {
        MessageBoxA(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // Show window
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}