#include <interception.h>
#include "utils.h"
#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <time.h>
#include <fstream>
#include <windows.h>
#include <armadillo>
#include <conio.h>

#pragma warning(disable : 4996)

using namespace std;
using namespace arma;

int
	RUNTIME = 30, // Set how long program will run for (without manual termination) in minutes- Default = 30 mins
	SMOOTH_AMT = 2; // Set level of smoothing (between 0-5)

double
	MAX_SENS = 2,    // maximum allowed sensitivity
	MIN_SENS = 0.5, // minimum allowed sensitivity

	SENS_MEAN = 1.0,  // initial mean value of sensitivity distribution
	SENS_SPREAD = 0.6, // initial standard deviation of sensitivity distribution
	TIMESTEP_MEAN = 0.2,    // initial mean value of timestep distribution
	TIMESTEP_STDDEV = 0.1,  // initial standard deviation of timestep distribution

	GAUSSIAN_STDDEV = 1000, // standard deviation of the smoothing window
	GAUSSIAN_WINDOW_SIZE = 5000; // size of the smoothing window

bool
	DEBUG = 1,
	VISUALIZE = 0,
	TYPE = 1;

COORD coord;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

void setUp()
{
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof cfi;
	cfi.nFont = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 14;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	wcscpy_s(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);

	coord.X = 80;
	coord.Y = 30;
	SetConsoleScreenBufferSize(hConsole, coord);

	SMALL_RECT conSize;

	conSize.Left = 0;
	conSize.Top = 0;
	conSize.Right = coord.X - 1;
	conSize.Bottom = coord.Y - 1;

	SetConsoleWindowInfo(hConsole, TRUE, &conSize);

	SetConsoleTextAttribute(hConsole, 0x0f);
	std::printf("Sensitivity Randomizer \n======================\n");
	std::printf("By: Whisper & El Bad\n\n");
	SetConsoleTextAttribute(hConsole, 0x08);
}

auto generateSensitivities()
{
	std::printf("Attempting to open settings file...\n");

	double variableValue;
	char variableName[100];
	bool garbageFile = 0;

	variableName[0] = '\0';
	FILE* fp;

	if ((fp = fopen("settings.ini", "r+")) == NULL)
	{
		SetConsoleTextAttribute(hConsole, 0x04);
		std::printf("Cannot read from settings file. Using defaults.\n");
		SetConsoleTextAttribute(hConsole, 0x08);
	}
	else
	{
		SetConsoleTextAttribute(hConsole, 0x02);
		std::printf("Settings file found! Reading values. ");
		SetConsoleTextAttribute(hConsole, 0x08);

		for (int i = 0; i < 99 && fscanf(fp, "%s = %lf", &variableName, &variableValue) != EOF; i++)
		{
			if (strcmp(variableName, "Smooth") == 0) { TYPE = variableValue; }
			else if (strcmp(variableName, "Baseline_Sensitivity") == 0) { SENS_MEAN = variableValue; }
			else if (strcmp(variableName, "Min_Sensitivity") == 0) { MIN_SENS = variableValue; }
			else if (strcmp(variableName, "Max_Sensitivity") == 0) { MAX_SENS = variableValue; }
			else if (strcmp(variableName, "Spread") == 0) { SENS_SPREAD = variableValue; }
			else if (strcmp(variableName, "Smoothing") == 0) { SMOOTH_AMT = variableValue; }
			else if (strcmp(variableName, "Timestep") == 0) { TIMESTEP_MEAN = variableValue; }
			else if (strcmp(variableName, "Runtime") == 0) { RUNTIME = variableValue; }
			else if (strcmp(variableName, "Visualize") == 0) { VISUALIZE = variableValue; }
			else if (strcmp(variableName, "Debug") == 0) { DEBUG = variableValue; }
			else { garbageFile = 1; }
		}
		fclose(fp);
	}

	// Check and correct for errors in settings.ini
	if (SENS_MEAN <= 0) { SENS_MEAN = 1; garbageFile = 1; }
	if (SENS_SPREAD <= 0) { SENS_SPREAD = 0.0000001; garbageFile = 1; } // Needs to be > 0
	if (SENS_MEAN < MIN_SENS) { MIN_SENS = SENS_MEAN / 2.0; garbageFile = 1; }
	if (SENS_MEAN > MAX_SENS) { MAX_SENS = SENS_MEAN * 2.0; garbageFile = 1; }
	if (SMOOTH_AMT < 0) { SMOOTH_AMT = 0; garbageFile = 1; }
	if (TIMESTEP_MEAN <= 0) { TIMESTEP_MEAN = 0.2; garbageFile = 1; }
	if (TYPE != 0 && TYPE != 1) { TYPE = 1; garbageFile = 1; }
	if (RUNTIME > 500) { RUNTIME = 500; } // Capping so as to not fry computer
	if (TYPE == 1) { TIMESTEP_MEAN = 0.2; TIMESTEP_STDDEV = 0.1; }
	if (TYPE == 0) { TIMESTEP_STDDEV = TIMESTEP_MEAN / 10.0; }

	if (garbageFile) 
	{ 
		SetConsoleTextAttribute(hConsole, 0x04);
		std::printf("Fixing errors!\n");
		SetConsoleTextAttribute(hConsole, 0x08);
	}
	else { std::printf("\n"); }

	// Set standard deviation of smoothing window
	switch (SMOOTH_AMT)
	{
		case 0: GAUSSIAN_STDDEV = 1;
		case 1: GAUSSIAN_STDDEV = 100;
		case 2: GAUSSIAN_STDDEV = 1000;
		case 3: GAUSSIAN_STDDEV = 5000;
		case 4: GAUSSIAN_STDDEV = 10000;
		case 5: GAUSSIAN_STDDEV = 50000;
	}

	string print_type;
	if (TYPE == 0) { print_type = "0 (Step)"; }
	else { print_type = "1 (Smoothed)"; }

	std::printf("\nYour settings are:\n");

	SetConsoleTextAttribute(hConsole, 0x02);
	std::printf("Type: %s\nBase Sensitivity: %.2f\nMin Sensitivity Multiplier: %.2f\nMax Sensitivity Multiplier: %.2f\nSpread: %.2f\nSmoothing: %d\nTimestep: %.1f seconds\nRuntime: %d minutes\nVisualize: %d\nDebug: %d\n\n", print_type.c_str(), SENS_MEAN, MIN_SENS, MAX_SENS, SENS_SPREAD, SMOOTH_AMT, TIMESTEP_MEAN, RUNTIME, VISUALIZE, DEBUG);
	SetConsoleTextAttribute(hConsole, 0x08);

	SetConsoleTextAttribute(hConsole, 0x4f);
	std::printf(" [CTRL+C] to QUIT ");
	SetConsoleTextAttribute(hConsole, 0x08);

	coord.X = 20;
	coord.Y = 19;

	SetConsoleCursorPosition(hConsole, coord);
	SetConsoleTextAttribute(hConsole, 0x5f);
	std::printf(" [P] to PAUSE ");
	SetConsoleTextAttribute(hConsole, 0x08);

	coord.X = 0;
	coord.Y = 21;

	SetConsoleCursorPosition(hConsole, coord);
	SetConsoleTextAttribute(hConsole, 0x08);

	std::printf("Generating Sensitivity Curve...");

	double
		random_sens,
		random_timestep,
		running_time = 0;

	vector<double> 
		x_vals,
		y_vals;

	// Populate vectors with the first timestep (0.0 seconds), and the inputted mean sensitivity
	x_vals.push_back(0.0);
	y_vals.push_back(SENS_MEAN);

	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	default_random_engine generator(seed);

	while (running_time < RUNTIME * 60.0)
	{
		// Generate a normal distribution around the mean
		lognormal_distribution<double> sens_distribution(std::log(SENS_MEAN), SENS_SPREAD);

		double random_sens = sens_distribution(generator);

		// Ensure outputted sensitivities are within the bounds set by the user
		if ((random_sens < MIN_SENS) || (random_sens > MAX_SENS))
		{
			// Iterate until a sensitivity within the range is achieved
			do {
				random_sens = sens_distribution(generator);
			} while ((random_sens < MIN_SENS) || (random_sens > MAX_SENS));
		}

		// Update the mean sensitivity to be the current random sensitivity
		SENS_MEAN = random_sens;

		// Generate a random new timestep
		normal_distribution<double> timestep_distribution(TIMESTEP_MEAN, TIMESTEP_STDDEV);
		random_timestep = timestep_distribution(generator);

		// Ensure the timestep is some positive value
		if (random_timestep <= 0)
		{
			do {
				random_timestep = timestep_distribution(generator);
			} while (random_timestep <= 0);
		}

		running_time += random_timestep;
		//cout << running_time << endl;

		// Append to vectors of sensitivities and timesteps
		x_vals.push_back(running_time);
		y_vals.push_back(random_sens);
	}

	auto vals = make_tuple(x_vals, y_vals);
	return vals;
}

auto smooth(vector<double>&x_vals, vector<double>&y_vals)
{
	// Generate a list of points to "connect" the random points
	// New point list linearly connects to each point after it

	vec xx = linspace<vec>(x_vals.front(), x_vals.back(), x_vals.size() * 200);
	vec yy;

	vec arma_x_vals(x_vals);
	vec arma_y_vals(y_vals);

	// Actual "interpolation" function to connect the points together
	arma::interp1(arma_x_vals, arma_y_vals, xx, yy);

	// Generate gaussian window used in convolution
	vec n = linspace<vec>(0, GAUSSIAN_WINDOW_SIZE - 1, GAUSSIAN_WINDOW_SIZE) - (GAUSSIAN_WINDOW_SIZE - 1) / 2.0;
	double sig2 = 2 * GAUSSIAN_STDDEV * GAUSSIAN_STDDEV;
	vec w = exp(-square(n) / sig2);

	// Convolve linearly connected y values with gaussian
	// "Smoothing" effect here
	vec smooth_curve = conv(yy, w / sum(w), "same");

	// Convert back to std::vector to be used in Interception
	vector<double> timesteps = conv_to < vector<double> >::from(xx);
	vector<double> sensitivities = conv_to< vector<double> >::from(smooth_curve);

	// Chop off artifacts from smoothing algorithm
	// Smoothed curve is janky around beginning and ends
	int off = 4000;
	vector<double> t(timesteps.begin() + off, timesteps.end() - off);
	vector<double> s(sensitivities.begin() + off, sensitivities.end() - off);

	// Previously chopped of first few seconds
	// Subtract off so that the values begin at 0.0 seconds again
	double offset = t.front();
	std::for_each(t.begin(), t.end(), [offset](double& d) { d -= offset; });

	auto vals = make_tuple(t, s);
	return vals;
}

auto visualize(vector<double>& t, vector<double>& s)
{
	ofstream sens_list;
	sens_list.open("sens_list.txt");

	// Used to plot legend in Matplotlib
	// Also used to determine y limits (between min and max sensitivity)
	sens_list << SENS_MEAN << "," << MIN_SENS << "," << MAX_SENS << "," << TYPE << "," << SENS_SPREAD << "," << SMOOTH_AMT << "," << RUNTIME << "," << VISUALIZE << "," << DEBUG << "\n";

	// Write coords as x, y to file
	for (int i = 0; i < t.size(); i++)
	{
		sens_list << t[i] << "," << s[i] << "\n";
	}
	sens_list.close();
}

int main()
{
	InterceptionContext context;
	InterceptionDevice device;
	InterceptionStroke stroke;

	DWORD prev_mode;
	GetConsoleMode(hConsole, &prev_mode);
	SetConsoleMode(hConsole, prev_mode & ~ENABLE_QUICK_EDIT_MODE);

	raise_process_priority();

	context = interception_create_context();

	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE);
	interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

	int i = 1;

	double
		dx,
		dy,
		passage,
		sens_multiplier,
		carryX = 0,
		carryY = 0,
		time = 0;

	vector<double>
		final_x,
		final_y;

	setUp();
	tie(final_x, final_y) = generateSensitivities();
	if (TYPE == 1) { tie(final_x, final_y) = smooth(final_x, final_y); }

	if (VISUALIZE) { visualize(final_x, final_y); }

	int size = final_x.size();

	typedef chrono::high_resolution_clock Time;
	typedef chrono::duration<float> fsec;

	auto prev_time = Time::now();
	auto curr_time = Time::now();

	sens_multiplier = final_y.front();

	static double percent;
	static bool paused = false;
	static DWORD lastpress = 0;

	if (DEBUG == 0) { std::printf("\nRunning...\n"); }

	while (interception_receive(context, device = interception_wait(context), &stroke, 1) > 0)
	{
		if (interception_is_mouse(device))
		{
			InterceptionMouseStroke& mstroke = *(InterceptionMouseStroke*)& stroke;
			if (!(mstroke.flags & INTERCEPTION_MOUSE_MOVE_ABSOLUTE)) {

				coord.X = 0;
				coord.Y = 20;

				fsec fs = curr_time - prev_time;
				passage = fs.count();

				if (i < size)
				{
					if (passage > final_x[i] - final_x[i-1])
					{
						if (!paused) 
						{
							int old_i = i - 1;
							while (passage > final_x[i] - final_x[old_i]){ i++; }

							sens_multiplier = final_y[i];
							percent = double(i) / size * 100;
							prev_time = Time::now();
							i++;
						}

						if (DEBUG == 1)
						{
							SetConsoleCursorPosition(hConsole, coord);
							SetConsoleTextAttribute(hConsole, 0x08);
							std::printf("\nCurrent Sensitivity Multiplier: ");

							SetConsoleTextAttribute(hConsole, 0xe0);
							std::printf("%.5f\n", sens_multiplier);
							SetConsoleTextAttribute(hConsole, 0x08);

							std::printf("Program Termination: ");

							SetConsoleTextAttribute(hConsole, 0xe0);
							std::printf("%.3f%%\n\n", percent);
							SetConsoleTextAttribute(hConsole, 0x08);
						}
					}
				}
				else { break; }

				dx = mstroke.x * sens_multiplier + carryX;
				dy = mstroke.y * sens_multiplier + carryY;

				carryX = dx - floor(dx);
				carryY = dy - floor(dy);

				mstroke.x = (int)floor(dx);
				mstroke.y = (int)floor(dy);

				time += passage;
			}

			interception_send(context, device, &stroke, 1);
			curr_time = Time::now();
		}

		if (interception_is_keyboard(device))
		{
			InterceptionKeyStroke& kstroke = *(InterceptionKeyStroke*)& stroke;
			interception_send(context, device, &stroke, 1);
			if (kstroke.code == 0x19 && (GetTickCount64() - lastpress > 250)) 
			{
				lastpress = GetTickCount64();
				paused = !paused;
			}

			coord.X = 36;
			coord.Y = 19;
			SetConsoleCursorPosition(hConsole, coord);

			if (paused)
			{
				SetConsoleTextAttribute(hConsole, 0x3f);
				std::printf(" PAUSED ");
				SetConsoleTextAttribute(hConsole, 0x08);
			}
			else
			{
				std::printf("        ");
				SetConsoleTextAttribute(hConsole, 0x08);
			}
		}
	}

	SetConsoleTextAttribute(hConsole, 0x02);
	std::printf("\n\n\n\n\nPlease restart the program to regenerate another smooth sensitivity curve.\n\n");
	SetConsoleTextAttribute(hConsole, 0x08);

	interception_destroy_context(context);

	return 0;
}
