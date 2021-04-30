#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//#include <atomic>
//#include <fstream>

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

// Instantiation of module.
Vtop* top = NULL;

// Video
// -----
//#define VGA_WIDTH 257
//#define VGA_HEIGHT 240
#define VGA_WIDTH 512
#define VGA_HEIGHT 512
int pix_count = 0;
SYSTEMTIME actualtime;
LONG time_ms;
LONG old_time = 0;
LONG frame_time = 0;
float fps = 0.0;
static int batchSize = 25000000 / 100;
unsigned char rgb[3];
bool prev_vsync = 0;
int frame_count = 0;
bool prev_hsync = 0;
int line_count = 0;

// Simulation control
// -----------
bool run_enable = 0;
bool single_step = 0;
bool multi_step = 0;
int multi_step_amount = 1024;

void ioctl_download_before_eval(void);
void ioctl_download_after_eval(void);

// Core inputs
// -----------
#define VSW1    top->top__DOT__sw1
#define VSW2    top->top__DOT__sw2
#define PLAYERINPUT top->top__DOT__playerinput
#define JS      top->top__DOT__joystick
void js_assert(int s) { JS &= ~(1 << s); }
void js_deassert(int s) { JS |= 1 << s; }
void playinput_assert(int s) { PLAYERINPUT &= ~(1 << s); }
void playinput_deassert(int s) { PLAYERINPUT |= (1 << s); }

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
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
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
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
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

uint32_t* disp_ptr = NULL;
uint32_t* vga_ptr = NULL;

unsigned int disp_size = VGA_WIDTH * VGA_HEIGHT * 4;
uint32_t vga_size = VGA_WIDTH * VGA_HEIGHT * 4;


DebugConsole console;

static void ShowDebugConsole(bool* p_open)
{
	
	console.Draw("Debug Log", p_open);
}



int verilate() {

	if (!Verilated::gotFinish()) {

		if (main_time < 48) {
			top->reset = 1;   	// Assert reset (active HIGH)
		}
		if (main_time == 48) {	// Do == here, so we can still reset it in the main loop.
			top->reset = 0;		// Deassert reset.
		}

		top->clk_vid = !top->clk_vid;
		top->clk_sys = !top->clk_sys;

		if (top->clk_vid == 1) {

			pix_count++;

			rgb[0] = top->VGA_B;
			rgb[1] = top->VGA_G;
			rgb[2] = top->VGA_R;
			uint32_t vga_addr = (line_count * VGA_WIDTH) + pix_count;

			bool draw = !(top->VGA_HB || top->VGA_VB);

			if (draw) {
				disp_ptr[vga_addr] = 0xFF000000 | rgb[0] << 16 | rgb[1] << 8 | rgb[2];	// Our debugger framebuffer is in the 32-bit RGBA format.
			}
			if (prev_hsync && !top->VGA_HS) {
				console.AddLog("HSYNC pixel: %d line: %d", pix_count, line_count);
				line_count++;
				pix_count = 0;
			}

			if (prev_vsync && !top->VGA_VS) {
				console.AddLog("VSYNC pixel: %d line: %d", pix_count, line_count);
				frame_count++;
				line_count = 0;
				pix_count = 0;

				GetSystemTime(&actualtime);
				time_ms = (actualtime.wSecond * 1000) + actualtime.wMilliseconds;

				frame_time = time_ms - old_time;
				old_time = time_ms;

				fps = 1000.0 / frame_time;

				console.AddLog("Frame: %06d  frame_time: %06d fps: %06f", frame_count, frame_time, fps);
			}

			prev_hsync = top->VGA_HS;
			prev_vsync = top->VGA_VS;
		}

		if (top->clk_sys) { ioctl_download_before_eval(); }
		/*else if (ioctl_file)
			printf("skipping download this cycle %d\n",top->clk_sys);*/

		top->eval();            // Evaluate model!

		if (top->clk_sys) { ioctl_download_after_eval(); }

		main_time++;            // Time passes...

		return 1;
	}
	// Stop Verilating...
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
	if (!ioctl_file) printf("error opening %s\n", file);
}
int nextchar = 0;
void ioctl_download_before_eval()
{
	if (ioctl_file) {
		//printf("ioctl_download_before_eval %x\n",top->ioctl_addr);
		if (top->ioctl_wait == 0) {
			top->ioctl_download = 1;
			top->ioctl_wr = 1;

			if (feof(ioctl_file)) {
				fclose(ioctl_file);
				ioctl_file = NULL;
				top->ioctl_download = 0;
				top->ioctl_wr = 0;
				printf("finished upload\n");

			}
			if (ioctl_file) {
				int curchar = fgetc(ioctl_file);
				if (feof(ioctl_file) == 0) {
					//	    		if (curchar!=EOF) {
									//top->ioctl_dout=(char)curchar;
					nextchar = curchar;
					//printf("ioctl_download_before_eval: dout %x \n",top->ioctl_dout);
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
		console.AddLog("ioctl_download_after_eval %x wr %x dl %x\n", top->ioctl_addr, top->ioctl_wr, top->ioctl_download); console;
	}
}

void start_load_image() {
	ioctl_download_setfile("..\\Image Examples\\bird.bin", 0);
}


int main(int argc, char** argv, char** env) {

	disp_ptr = (uint32_t*)malloc(disp_size);
	vga_ptr = (uint32_t*)malloc(vga_size);


#ifdef WIN32
	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
	RegisterClassEx(&wc);
	HWND hwnd = CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1600, 1100, NULL, NULL, wc.hInstance, NULL);

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
		printf("Error: %s\n", SDL_GetError());
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

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

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

	memset(disp_ptr, 0xAA, disp_size);
	memset(vga_ptr, 0xAA, vga_size);

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Build texture atlas
	int width = VGA_WIDTH;
	int height = VGA_HEIGHT;

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
	subResource.pSysMem = disp_ptr;
	//subResource.pSysMem = vga_ptr;
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


	//    ImTextureID my_tex_id = (ImTextureID) renderedTexture;
#endif

	static bool show_app_console = true;

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

		ShowDebugConsole(&show_app_console);

		ImGui::Begin("Virtual Dev Board v1.0");		// Create a window called "Virtual Dev Board v1.0" and append into it.
		ImGui::SetWindowPos("Virtual Dev Board v1.0", ImVec2(600, 40), ImGuiCond_Once);
		ImGui::SetWindowSize("Virtual Dev Board v1.0", ImVec2(900, 900), ImGuiCond_Once);
		if (ImGui::Button("RESET")) { top->reset = 1; main_time = 0; }
		ImGui::Text("main_time: %d frame_count: %d", main_time, frame_count);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::Checkbox("RUN", &run_enable);
		if (single_step == 1) { single_step = 0; }
		if (ImGui::Button("Single Step")) { run_enable = 0; single_step = 1; }
		if (multi_step == 1) { multi_step = 0; }
		if (ImGui::Button("Multi Step")) { run_enable = 0; multi_step = 1; }
		ImGui::SameLine(); ImGui::SliderInt("Step amount", &multi_step_amount, 8, 100024);
		ImGui::Image(my_tex_id, ImVec2(width * 1.25f, height * 1.25f));
		ImGui::End();
#if 1
		VSW1 = 0x54;
		VSW2 = 0x0;
		PLAYERINPUT = 0x3df;
		JS = 0x0;
#endif

#ifdef WIN32
		// Update the texture!
		// D3D11_USAGE_DEFAULT MUST be set in the texture description (somewhere above) for this to work.
		// (D3D11_USAGE_DYNAMIC is for use with map / unmap.) ElectronAsh.

		g_pd3dDeviceContext->UpdateSubresource(pTexture, 0, NULL, disp_ptr, width * 4, 0);

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
	return 0;
}
