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
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#else
#define WIN32
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#endif

#include "../imgui/imgui_memory_editor.h"

#include <sim_console.h>
#include <sim_video.h>

FILE* ioctl_file = NULL;
int ioctl_next_addr = 0x0;

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

#define VGA_WIDTH 240
#define VGA_HEIGHT 257

SimVideo video(VGA_WIDTH, VGA_HEIGHT);

int pix_count = 0;
int line_count = 0;
int frame_count = 0;
bool prev_hsync = 0;
bool prev_vsync = 0;
bool prev_hblank = 0;
bool prev_vblank = 0;
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
void js_assert(int s) { JS &= ~(1 << s); }
void js_deassert(int s) { JS |= 1 << s; }
void playinput_assert(int s) { PLAYERINPUT &= ~(1 << s); }
void playinput_deassert(int s) { PLAYERINPUT |= (1 << s); }

const int input_right = 0;
const int input_left = 1;
const int input_down = 2;
const int input_up = 3;
const int input_fire1 = 4;
const int input_start_1 = 5;
const int input_start_2 = 6;
const int input_coin_1 = 7;

bool inputs[8];

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

vluint64_t main_time = 0;	// Current simulation time.
double sc_time_stamp() {	// Called by $time in Verilog.
	return main_time;
}

DebugConsole console;

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

			// Only draw outside of blanks
			if (!(top->VGA_HB || top->VGA_VB)) {
				video.Output(pix_count - 1, line_count, 0xFF000000 | top->VGA_B << 16 | top->VGA_G << 8 | top->VGA_R);
			}

			// Increment pixel counter
			pix_count++;

			// Falling edge of hblank
			if (prev_hblank && !top->VGA_HB) {
				// Increment line and reset pixel count
				line_count++;
				pix_count = 0;
			}

			// Falling edge of vblank
			if (prev_vblank && !top->VGA_VB) {
				frame_count++;
				line_count = 0;

				GetSystemTime(&actualtime);
				time_ms = (actualtime.wSecond * 1000) + actualtime.wMilliseconds;
				frame_time = time_ms - old_time;
				old_time = time_ms;
				fps = 1000.0 / frame_time;
			}
			prev_hsync = top->VGA_HS;
			prev_vsync = top->VGA_VS;
			prev_hblank = top->VGA_HB;
			prev_vblank = top->VGA_VB;
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

	if (video.Initialise(windowTitle) == 1) { return 1; }

	// Create core and initialise
	top = new Vtop();
	Verilated::commandArgs(argc, argv);

#ifdef WIN32
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
#else
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
		}
#endif
		video.StartFrame();


		// Player inputs
		// -------------
		// Read keyboard state
		bool pr = ReadKeyboard();

		inputs[input_up] = m_keyboardState[DIK_UP] & 0x80;
		inputs[input_right] = m_keyboardState[DIK_RIGHT] & 0x80;
		inputs[input_down] = m_keyboardState[DIK_DOWN] & 0x80;
		inputs[input_left] = m_keyboardState[DIK_LEFT] & 0x80;
		inputs[input_fire1] = m_keyboardState[DIK_SPACE] & 0x80;
		inputs[input_start_1] = m_keyboardState[DIK_1] & 0x80;
		inputs[input_coin_1] = m_keyboardState[DIK_5] & 0x80;



		// Draw GUI
		// --------
		ImGui::NewFrame();

		console.Draw("Debug Log", &showDebugWindow);
		ImGui::Begin(debugWindowTitle);
		ImGui::SetWindowPos(debugWindowTitle, ImVec2(580, 10), ImGuiCond_Once);
		ImGui::SetWindowSize(debugWindowTitle, ImVec2(1000, 1000), ImGuiCond_Once);

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
		ImGui::Text("ms/frame: %.3f FPS: %.1f", 1000.0f / video.io.Framerate, video.io.Framerate);
		ImGui::Text("xMax: %d  yMax: %d", video.xMax, video.yMax);
		ImGui::Text("xMin: %d  yMin: %d", video.xMin, video.yMin);

		//ImGui::Text("up: %x", PLAYERINPUT[&input_up]); ImGui::SameLine();
		//ImGui::Text("down: %x", PLAYERINPUT[&input_down]); ImGui::SameLine();
		//ImGui::Text("left: %x", PLAYERINPUT[&input_left]); ImGui::SameLine();
		//ImGui::Text("right: %x", PLAYERINPUT[&input_right]);

		ImGui::Text("start 1: %x", inputs[input_start_1]); ImGui::SameLine();
		ImGui::Text("coin 1: %x", inputs[input_coin_1]);


		float m = 3.0;
		ImGui::Image(video.texture_id, ImVec2(video.output_width * m, video.output_height * m));
		ImGui::End();

		ImGui::Begin("RAM Editor");
		memoryEditor_hs.DrawContents(top->top__DOT__uut__DOT__hs_ram__DOT__mem, 64, 0);
		ImGui::End();

		video.UpdateTexture();

		//		// Set core initial inputs
					//#if 1
					//		VSW1 = 0x54;
					//		VSW2 = 0x0;
					//		PLAYERINPUT = 0x3df;
					//		JS = 0x0;
					//#endif



		uint16_t i = 0x3df;
		//i = 0x3db;
		//PLAYERINPUT = 0x3df;
		//if (inputs[input_fire1]) { playinput_assert(9);  console.AddLog("FIRE 1"); }
		//else { playinput_deassert(9); };
		//PLAYERINPUT &= ~(0 << 8);
		//if (inputs[input_coin_1]) { playinput_assert(7); console.AddLog("COIN 1"); }
		//else { playinput_deassert(7); };
		////PLAYERINPUT &= ~(0 << 6);
		////PLAYERINPUT &= ~(0 << 5);
		////PLAYERINPUT &= ~(1 << 4);
		////PLAYERINPUT &= ~(1 << 3);
		if (inputs[input_start_1]) {
			i = 0x3df;
			//i &= ~(1 << 2); 
		}
		//if (inputs[input_start_1]) { playinput_assert(2);  console.AddLog("START 1"); }
		//else { playinput_deassert(3); };
		////if (inputs[input_fire2]) { playinput_assert(1);  console.AddLog("FIRE 2"); }
		////else { playinput_deassert(1); };
		//if (inputs[input_fire1]) { playinput_assert(0);  console.AddLog("FIRE 1"); }
		//else { playinput_deassert(0); };

		top->top__DOT__playerinput = i;
		//PLAYERINPUT = i;

		//= { 1'b1, 1'b1, ~(m_coin), m_test, status[12], m_slam, ~(m_start2), ~(m_start1), ~m_fire_2, ~m_fire };
		//assign joystick_i = { ~m_right,~m_left,~m_down,~m_up, ~m_right_2,~m_left_2,~m_down_2,~m_up_2 };

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

	video.CleanUp();

	// Release keyboard
	if (m_keyboard) { m_keyboard->Unacquire(); m_keyboard->Release(); m_keyboard = 0; }
	// Release direct input
	if (m_directInput) { m_directInput->Release(); m_directInput = 0; }

	return 0;
}
