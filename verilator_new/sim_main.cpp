#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <verilated.h>
#include "Vtop.h"

#include "imgui.h"
#ifndef _MSC_VER

#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>

#else
#define WIN32
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#endif

#include "../imgui/imgui_memory_editor.h"

#include <sim_console.h>

FILE* ioctl_file = NULL;
int ioctl_next_addr = 0x0;


#ifndef WIN32
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;
#else
// DirectX data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGIFactory* g_pFactory = NULL;
static ID3D11Buffer* g_pVB = NULL;
static ID3D11Buffer* g_pIB = NULL;
static ID3D10Blob* g_pVertexShaderBlob = NULL;
static ID3D11VertexShader* g_pVertexShader = NULL;
static ID3D11InputLayout* g_pInputLayout = NULL;
static ID3D11Buffer* g_pVertexConstantBuffer = NULL;
static ID3D10Blob* g_pPixelShaderBlob = NULL;
static ID3D11PixelShader* g_pPixelShader = NULL;
static ID3D11SamplerState* g_pFontSampler = NULL;
static ID3D11ShaderResourceView* g_pFontTextureView = NULL;
static ID3D11RasterizerState* g_pRasterizerState = NULL;
static ID3D11BlendState* g_pBlendState = NULL;
static ID3D11DepthStencilState* g_pDepthStencilState = NULL;
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;
#endif

// GUI 
const char* windowTitle = "Verilator Sim: Arcade-Centipede";
bool showDebugWindow = true;
const char* debugWindowTitle = "Virtual Dev Board v1.0";
MemoryEditor memoryEditor_hs;

// DirectInput
#define DIRECTINPUT_VERSION 0x0800
IDirectInput8* m_directInput;
IDirectInputDevice8* m_keyboard;
unsigned char m_keyboardState[256];

// Video
// -----
#define TEX_WIDTH 240
#define TEX_HEIGHT 257
#define VGA_WIDTH 257
#define VGA_HEIGHT 240
int pix_count = 0;
int line_count = 0;
int frame_count = 0;
bool prev_hsync = 0;
bool prev_vsync = 0;
static int batchSize = 25000000 / 100;

// Statistics
// ----------
SYSTEMTIME actualtime;
LONG time_ms;
LONG old_time = 0;
LONG frame_time = 0;
float fps = 0.0;
int initialReset = 48;

// Simulation control
// ------------------
bool run_enable = 1;
bool single_step = 0;
bool multi_step = 0;
int multi_step_amount = 1024;

void ioctl_download_before_eval(void);
void ioctl_download_after_eval(void);


// Verilog module
// --------------
Vtop* top = NULL;

// - Core inputs
#define VSW1    top->top__DOT__sw1
#define VSW2    top->top__DOT__sw2
#define PLAYERINPUT top->top__DOT__playerinput
#define JS      top->top__DOT__joystick

const int input_right = 0;
const int input_left = 1;
const int input_down = 2;
const int input_up = 3;
const int input_fire1 = 4;
const int input_start_1 = 5;
const int input_start_2 = 6;
const int input_coin_1 = 7;

bool inputs[8];

void js_assert(int s) { inputs[s] = 1; JS &= ~(1 << s); }
void js_deassert(int s) { inputs[s] = 1;  JS |= 1 << s; }
void input_assert(int s) { inputs[s] = 1;  PLAYERINPUT &= ~(1 << s); }
void input_deassert(int s) { inputs[s] = 0; PLAYERINPUT |= (1 << s); }


#ifdef WIN32
// Data
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

HRESULT CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = TEX_WIDTH;
	sd.BufferDesc.Height = TEX_HEIGHT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return E_FAIL;
	CreateRenderTarget();
	return S_OK;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
#else
#endif

vluint64_t main_time = 0;	// Current simulation time.
double sc_time_stamp() {	// Called by $time in Verilog.
	return main_time;
}

uint32_t* video_ptr = NULL;
unsigned int video_size = TEX_WIDTH * TEX_HEIGHT * 4;

DebugConsole console;

int xMax = -100;
int yMax = -100;
int xMin = 1000;
int yMin = 1000;

// This is really not relevant, just a reminder
int clockSpeed = 24;

int clk_ratio_sys = 1;
int clk_ratio_pix = 2;

int clk_sys;
int clk_div_sys;

int clk_pix;
int clk_pix_old;
int clk_div_pix;


void resetSimulation() {
	main_time = 0;
	top->reset = 1;
	clk_div_sys = 0;
	clk_sys = 0;
	clk_div_pix = 0;
	clk_pix = 0;
	clk_pix_old = 0;
}


int verilate() {

	if (!Verilated::gotFinish()) {

		// Assert reset during startup
		if (main_time < initialReset) { top->reset = 1; }
		// Deassert reset after startup
		if (main_time == initialReset) { top->reset = 0; }

		// Clock dividers
		clk_div_sys++;
		if (clk_div_sys >= clk_ratio_sys) { clk_sys = !clk_sys; clk_div_sys = 0; }
		clk_div_pix++;
		if (clk_div_pix >= clk_ratio_pix) { clk_pix = !clk_pix;	clk_div_pix = 0; }

		// Set system clock in core
		top->clk_12 = clk_sys;

		// Rising edge of pixel clock
		if (clk_pix == 1 && clk_pix_old == 0) {

			// TODO: Fix hardcoded offsets
			//int yo = -63;
			int yo = -95;
			int xo = -14;

			// Rotated output by 90 degrees, flipped vertically!
			int y = TEX_HEIGHT - (pix_count + yo);
			int xs = TEX_WIDTH;
			int x = line_count + xo;

			// Clamp values to stop access violations on texture
			if (x < 0) { x = 0; }
			if (x > TEX_WIDTH - 1) { x = TEX_WIDTH - 1; }
			if (y < 0) { y = 0; }
			if (y > TEX_HEIGHT - 1) { y = TEX_HEIGHT - 1; }

			// Only draw to texture if outside either hblank or vblank
			bool draw = (!(top->VGA_HB || top->VGA_VB));
			if (draw) {
				// Set colour to core output
				uint32_t colour = 0xFF000000 | top->VGA_B << 16 | top->VGA_G << 8 | top->VGA_R;
				// Generate texture address
				uint32_t vga_addr = (y * xs) + x;
				// Write pixel to texture
				video_ptr[vga_addr] = colour;	// Our debugger framebuffer is in the 32-bit RGBA format.
			}

			// Track bounds (debug)
			if (x > xMax) { xMax = x; }
			if (y > yMax) { yMax = y; }
			if (x < xMin) { xMin = x; }
			if (y < yMin) { yMin = y; }

			// Increment pixel counter
			pix_count++;

			// Rising edge of hsync
			if (!prev_hsync && top->VGA_HS) {
				// Increment line and reset pixel count
				line_count++;
				pix_count = 0;
			}
			prev_hsync = top->VGA_HS;

			// Rising edge of vsync
			if (!prev_vsync && top->VGA_VS) {
				//console.AddLog("VSYNC pixel: %d line: %d", pix_count, line_count);
				frame_count++;
				line_count = 0;

				GetSystemTime(&actualtime);
				time_ms = (actualtime.wSecond * 1000) + actualtime.wMilliseconds;
				frame_time = time_ms - old_time;
				old_time = time_ms;
				fps = 1000.0 / frame_time;
				//console.AddLog("Frame: %06d  frame_time: %06d fps: %06f", frame_count, frame_time, fps);
			}
			prev_vsync = top->VGA_VS;
		}
		clk_pix_old = clk_pix;

		if (clk_div_sys == 0) {
			if (clk_sys) { ioctl_download_before_eval(); }
			top->eval();  // Only evaluate verilog on change of the system clock
			if (clk_sys) { ioctl_download_after_eval(); }
		}

		main_time++;

		return 1;
	}
	// Stop verilating and cleanup
	top->final();
	delete top;
	exit(0);
	return 0;
}

void ioctl_download_setfile(char* file, int index)
{
	ioctl_next_addr = -1;
	top->ioctl_addr = ioctl_next_addr;
	top->ioctl_index = index;
	ioctl_file = fopen(file, "rb");
	if (!ioctl_file) {
		console.AddLog("error opening %s\n", file);
	}
}
int nextchar = 0;
void ioctl_download_before_eval()
{
	if (ioctl_file) {
		console.AddLog("ioctl_download_before_eval %x\n", top->ioctl_addr);
		if (top->ioctl_wait == 0) {
			top->ioctl_download = 1;
			top->ioctl_wr = 1;
			if (feof(ioctl_file)) {
				fclose(ioctl_file);
				ioctl_file = NULL;
				top->ioctl_download = 0;
				top->ioctl_wr = 0;
				console.AddLog("finished upload\n");
			}
			if (ioctl_file) {
				int curchar = fgetc(ioctl_file);
				if (feof(ioctl_file) == 0) {
					nextchar = curchar;
					console.AddLog("ioctl_download_before_eval: dout %x \n", top->ioctl_dout);
					ioctl_next_addr++;
				}
			}
		}
	}
	else {
		top->ioctl_download = 0;
		top->ioctl_wr = 0;
	}
}

void ioctl_download_after_eval()
{
	top->ioctl_addr = ioctl_next_addr;
	top->ioctl_dout = (unsigned char)nextchar;
	if (ioctl_file) {
		console.AddLog("ioctl_download_after_eval %x wr %x dl %x\n", top->ioctl_addr, top->ioctl_wr, top->ioctl_download);
	}
}

//void start_load_image() {
//	ioctl_download_setfile("..\\Image Examples\\bird.bin", 0);
//}

bool ReadKeyboard()
{
	HRESULT result;

	// Read the keyboard device.
	result = m_keyboard->GetDeviceState(sizeof(m_keyboardState), (LPVOID)&m_keyboardState);
	if (FAILED(result))
	{
		// If the keyboard lost focus or was not acquired then try to get control back.
		if ((result == DIERR_INPUTLOST) || (result == DIERR_NOTACQUIRED)) { m_keyboard->Acquire(); }
		else { return false; }
	}
	return true;
}

int main(int argc, char** argv, char** env) {

	m_directInput = 0;
	m_keyboard = 0;
	HRESULT result;

	// Initialize the main direct input interface.
	result = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&m_directInput, NULL);
	if (FAILED(result)) { return false; }
	// Initialize the direct input interface for the keyboard.
	result = m_directInput->CreateDevice(GUID_SysKeyboard, &m_keyboard, NULL);
	if (FAILED(result)) { return false; }
	// Set the data format.  In this case since it is a keyboard we can use the predefined data format.
	result = m_keyboard->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(result)) { return false; }
	// Now acquire the keyboard.
	result = m_keyboard->Acquire();
	if (FAILED(result)) { return false; }

	// Attach debug console to the verilated code
	Verilated::setDebug(console);

	// Setup pointers for video texture
	video_ptr = (uint32_t*)malloc(video_size);

#ifdef WIN32
	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
	RegisterClassEx(&wc);
	HWND hwnd = CreateWindow(wc.lpszClassName, _T(windowTitle), WS_OVERLAPPEDWINDOW, 100, 100, 1600, 1100, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (CreateDeviceD3D(hwnd) < 0)
	{
		CleanupDeviceD3D();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);
#else
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		console.AddLog("Error: %s\n", SDL_GetError());
		return -1;
	}
	top = new Vtop();

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync
#endif

	top = new Vtop();
	// Set core initial inputs
#if 1
	VSW1 = 0x54;
	VSW2 = 0x0;
	PLAYERINPUT = 0x3df;
	JS = 0x0;
#endif

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

#ifdef WIN32
	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
#else
	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL2_Init();

#endif

	Verilated::commandArgs(argc, argv);

	memset(video_ptr, 0xAA, video_size);

	// Our state
	ImVec4 clear_color = ImVec4(0.25f, 0.35f, 0.40f, 0.80f);

	// Build texture atlas
	int width = TEX_WIDTH;
	int height = TEX_HEIGHT;

#ifdef WIN32
	// Upload texture to graphics system
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = video_ptr;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &g_pFontTextureView);
	pTexture->Release();

	// Store our identifier
	ImTextureID my_tex_id = (ImTextureID)g_pFontTextureView;

	// Create texture sampler
	{
		D3D11_SAMPLER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0.f;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MinLOD = 0.f;
		desc.MaxLOD = 0.f;
		g_pd3dDevice->CreateSamplerState(&desc, &g_pFontSampler);
	}
#else
	// the texture should match the GPU so it doesn't have to copy
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VGA_WIDTH, VGA_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, disp_ptr);
	ImTextureID my_tex_id = (ImTextureID)tex;
#endif


#ifdef WIN32
	// imgui Main loop stuff...
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
#else
	// Main loop
	bool done = false;
	while (!done)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);

#endif

		ImGui::NewFrame();

		console.Draw("Debug Log", &showDebugWindow);

		ImGui::Begin(debugWindowTitle);
		ImGui::SetWindowPos(debugWindowTitle, ImVec2(600, 40), ImGuiCond_Once);
		ImGui::SetWindowSize(debugWindowTitle, ImVec2(900, 900), ImGuiCond_Once);

		if (ImGui::Button("RESET")) { resetSimulation(); } ImGui::SameLine();
		if (ImGui::Button("START")) { run_enable = 1; } ImGui::SameLine();
		if (ImGui::Button("STOP")) { run_enable = 0; } ImGui::SameLine();
		ImGui::Checkbox("RUN", &run_enable);
		ImGui::SliderInt("Batch size", &batchSize, 1, 1000000);

		if (single_step == 1) { single_step = 0; }
		if (ImGui::Button("Single Step")) { run_enable = 0; single_step = 1; }
		ImGui::SameLine();
		if (multi_step == 1) { multi_step = 0; }
		if (ImGui::Button("Multi Step")) { run_enable = 0; multi_step = 1; }
		ImGui::SameLine();

		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.25f);
		ImGui::SliderInt("Step amount", &multi_step_amount, 8, 1024);

		ImGui::Text("main_time: %d frame_count: %d", main_time, frame_count); ImGui::SameLine();
		ImGui::Text("ms/frame: %.3f FPS: %.1f", 1000.0f / io.Framerate, io.Framerate);
		ImGui::Text("xMax: %d  yMax: %d", xMax, yMax);
		ImGui::Text("xMin: %d  yMin: %d", xMin, yMin);

		ImGui::Text("up: %x", PLAYERINPUT[&input_up]); ImGui::SameLine();
		ImGui::Text("down: %x", PLAYERINPUT[&input_down]); ImGui::SameLine();
		ImGui::Text("left: %x", PLAYERINPUT[&input_left]); ImGui::SameLine();
		ImGui::Text("right: %x", PLAYERINPUT[&input_right]);

		ImGui::Text("start 1: %x", PLAYERINPUT[&input_start_1]); ImGui::SameLine();
		ImGui::Text("coin 1: %x", PLAYERINPUT[&input_coin_1]);


		float m = 2.5;
		ImGui::Image(my_tex_id, ImVec2(width * m, height * m));
		ImGui::End();

		ImGui::Begin("RAM Editor");
		memoryEditor_hs.DrawContents(top->top__DOT__uut__DOT__hs_ram__DOT__mem, 64, 0);
		ImGui::End();

#ifdef WIN32
		// Update the texture!
		// D3D11_USAGE_DEFAULT MUST be set in the texture description (somewhere above) for this to work.
		// (D3D11_USAGE_DYNAMIC is for use with map / unmap.) ElectronAsh.

		g_pd3dDeviceContext->UpdateSubresource(pTexture, 0, NULL, video_ptr, width * 4, 0);

		// Rendering
		ImGui::Render();
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_pSwapChain->Present(0, 0); // Present without vsync
#else

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VGA_WIDTH, VGA_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, disp_ptr);

		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		//glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
#endif


		// Player inputs
		// -------------
		// Read the current state of the keyboard.
		bool pr = ReadKeyboard();

		inputs[input_up] = m_keyboardState[DIK_UP] & 0x80;
		inputs[input_right] = m_keyboardState[DIK_RIGHT] & 0x80;
		inputs[input_down] = m_keyboardState[DIK_DOWN] & 0x80;
		inputs[input_left] = m_keyboardState[DIK_LEFT] & 0x80;

		inputs[input_fire1] = m_keyboardState[DIK_SPACE] & 0x80;
		inputs[input_start_1] = m_keyboardState[DIK_1] & 0x80;
		inputs[input_coin_1] = m_keyboardState[DIK_5] & 0x80;

		PLAYERINPUT = PLAYERINPUT &= (1 >> input_fire1);
		PLAYERINPUT = PLAYERINPUT &= (1 >> input_fire1);
		PLAYERINPUT = PLAYERINPUT &= (1 >> input_fire1);

		//= { 1'b1, 1'b1, ~(m_coin), m_test, status[12], m_slam, ~(m_start2), ~(m_start1), ~m_fire_2, ~m_fire };
		//assign joystick_i = { ~m_right,~m_left,~m_down,~m_up, ~m_right_2,~m_left_2,~m_down_2,~m_up_2 };



		//if (ImGui::IsKeyPressed(SDL_SCANCODE_SPACE)) {
		//	playinput_assert(0);
		//}
		//else {
		//	
		//}
		//if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT)) {
		//	js_assert(6);
		//}
		//else {
		//	js_deassert(6);
		//}
		//if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT)) {
		//	js_assert(7);
		//}
		//else {
		//	js_deassert(7);
		//}
		//if (ImGui::IsKeyPressed(SDL_SCANCODE_5)) {
		//	playinput_assert(7);
		//}
		//else
		//	playinput_deassert(7);
		//if (ImGui::IsKeyPressed(SDL_SCANCODE_1))
		//	playinput_assert(3);
		//else
		//	playinput_deassert(3);


		if (run_enable) {
			for (int step = 0; step < batchSize; step++) { verilate(); }
		}
		else {
			if (single_step) { verilate(); }
			if (multi_step) {
				for (int step = 0; step < multi_step_amount; step++) { verilate(); }
			}
		}
	}

	// Clean up before exit
	// --------------------

#ifdef WIN32
	// Close imgui stuff properly...
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
#else
	// Cleanup
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();
#endif

	// Release keyboard
	if (m_keyboard) { m_keyboard->Unacquire(); m_keyboard->Release(); m_keyboard = 0; }
	// Release direct input
	if (m_directInput) { m_directInput->Release(); m_directInput = 0; }

	return 0;
	}
