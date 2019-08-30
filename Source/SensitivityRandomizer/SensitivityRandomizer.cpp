#include <interception.h>
#include "utils.h"
#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <tuple>
#include <math.h>
#include <fstream>
#include <windows.h>
#include <armadillo>

#pragma warning(disable : 4996)

using namespace std;
using namespace arma;

enum ScanCode
{
	SCANCODE_P = 0x19
};

COORD coord;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

int
	ITERATIONS = 5000,  // Set effective "length" the program will run for
	SMOOTH_AMT = 2;

double
	MAX_SENS = 4,    // maximum allowed sensitivity
	MIN_SENS = 0.25, // minimum allowed sensitivity

	SENS_MEAN = 1.0,  // initial mean value of sensitivity distribution
	SENS_SPREAD = 0.6, // initial standard deviation of sensitivity distribution
	SENS_SKEW = 0.0, 
	TIMESTEP_MEAN = 0.2,    // initial mean value of timestep distribution
	TIMESTEP_STDDEV = 0.1,  // initial standard deviation of timestep distribution

	GAUSSIAN_STDDEV = 1000, // standard deviation of the smoothing window
	GAUSSIAN_WINDOW_SIZE = 5000; // size of the smoothing window
	

bool DEBUG = 1;
bool VISUALIZE = 0;

double DEFAULT_SENS = SENS_MEAN;

auto generateSensitivities()
{

	//=========================================// 
	//	    Set up command line interface      //
	//=========================================//

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
	std::printf("Smoothed Randomized Sensitivity \n===============================\n\n");
	SetConsoleTextAttribute(hConsole, 0x08);

	std::printf("Attempting to open settings file...\n");

	double variableValue;

	char variableName[100];
	variableName[0] = '\0';

	bool garbageFile = 0;

	//=========================================// 
	//  Read in values from settings.ini file  //
	//=========================================//

	FILE* fp;

	if ((fp = fopen("settings.ini", "r+")) == NULL) {
		SetConsoleTextAttribute(hConsole, 0x04);
		std::printf("Cannot read from settings file. Using defaults.\n");
		SetConsoleTextAttribute(hConsole, 0x08);
	}
	else {

		std::printf("Settings file found! Reading values.\n");
		for (int i = 0; i < 99 && fscanf(fp, "%s = %lf", &variableName, &variableValue) != EOF; i++) {

			if (strcmp(variableName, "Baseline_Sensitivity") == 0) { SENS_MEAN = variableValue; }
			else if (strcmp(variableName, "Min_Sensitivity") == 0) { MIN_SENS = variableValue; }
			else if (strcmp(variableName, "Max_Sensitivity") == 0) { MAX_SENS = variableValue; }
			else if (strcmp(variableName, "Spread") == 0) { SENS_SPREAD = variableValue; }
			else if (strcmp(variableName, "Skew") == 0) { SENS_SKEW = variableValue; }
			else if (strcmp(variableName, "Smoothing") == 0) { SMOOTH_AMT = variableValue; }
			else if (strcmp(variableName, "Iterations") == 0) { ITERATIONS = variableValue; }
			else if (strcmp(variableName, "Visualize") == 0) { VISUALIZE = variableValue; }
			else if (strcmp(variableName, "Debug") == 0) { DEBUG = variableValue; }
		}
		fclose(fp);
	}

	if ((SENS_MEAN < MIN_SENS) || (SENS_MEAN > MAX_SENS)) {
		MIN_SENS = SENS_MEAN / 2.0;
		MAX_SENS = SENS_MEAN * 2.0;
		SetConsoleTextAttribute(hConsole, 0x04);
		std::printf("Min or max out of range. Correcting automatically...\n");
		SetConsoleTextAttribute(hConsole, 0x08);
		garbageFile = 1;
	}
	if ((SENS_MEAN + SENS_SKEW < MIN_SENS) || (SENS_MEAN + SENS_SKEW > MAX_SENS)) {
		if (SENS_SKEW < 0) { 
			SENS_SKEW = MIN_SENS - SENS_MEAN;
			SetConsoleTextAttribute(hConsole, 0x04);
			std::printf("Skew value to small. Smallest skew value with your settings is: %f.\nCorrecting automatically...\n", SENS_SKEW);
			SetConsoleTextAttribute(hConsole, 0x08);
		}
		else { 
			SENS_SKEW = MAX_SENS - SENS_MEAN; 
			std::printf("\nSkew value too large. Largest skew value with your settings is %f.\nCorrecting automatically...\n:", SENS_SKEW);
			SetConsoleTextAttribute(hConsole, 0x08);
		}
		garbageFile = 1;
	}

	if (!garbageFile) {
		SetConsoleTextAttribute(hConsole, 0x02);
		printf("\nSettings file valid.\n\n");
		SetConsoleTextAttribute(hConsole, 0x08);
	}

	if (SENS_SPREAD <= 0) { SENS_SPREAD = 0.0000001; }

	if (SMOOTH_AMT <= 0) { GAUSSIAN_STDDEV = 1; }
	else if (SMOOTH_AMT == 1) { GAUSSIAN_STDDEV = 100; }
	else if (SMOOTH_AMT == 2) { GAUSSIAN_STDDEV = 1000; }
	else if (SMOOTH_AMT == 3) { GAUSSIAN_STDDEV = 5000; }
	else if (SMOOTH_AMT == 4) { GAUSSIAN_STDDEV = 10000; }
	else { GAUSSIAN_STDDEV = 50000; }

	std::printf("\nYour settings are:\n");

	SetConsoleTextAttribute(hConsole, 0x02);
	std::printf("Base Sensitivity: %.2f\nMin Sensitivity Multiplier: %.2f\nMax Sensitivity Multiplier: %.2f\nSpread: %.2f\nSkew: %.2f\nSmoothing: %d\nIterations: %d\nVisualize: %d\nDebug: %d\n\n", SENS_MEAN, MIN_SENS, MAX_SENS, SENS_SPREAD, SENS_SKEW, SMOOTH_AMT, ITERATIONS, VISUALIZE, DEBUG);
	SetConsoleTextAttribute(hConsole, 0x08);

	SetConsoleTextAttribute(hConsole, 0x4f);
	std::printf(" [CTRL+C] to QUIT ");
	SetConsoleTextAttribute(hConsole, 0x08);

	coord.X = 0;
	coord.Y = 22;
	SetConsoleCursorPosition(hConsole, coord);
	SetConsoleTextAttribute(hConsole, 0x08);

	std::printf("Generating Sensitivity Curve...");

	//=========================================// 
    //        Begin smoothing algorithm        //
    //=========================================//

	double
		random_sens,
		random_timestep,
		d = SENS_SKEW / sqrt(1 + (SENS_SKEW * SENS_SKEW)); // skew factor 

	vector<double> x_vals(ITERATIONS), y_vals(ITERATIONS);

	// Populate vectors with the first timestep (0.0 seconds), and the inputted mean sensitivity
	x_vals.push_back(0.0);
	y_vals.push_back(SENS_MEAN);

	default_random_engine generator;

	for (int i = 0; i < ITERATIONS; i++)
	{
		// Generate a normal distribution around the mean
		normal_distribution<double> sens_distribution(SENS_MEAN, SENS_SPREAD);
		
		double r1 = sens_distribution(generator);
		double r2 = sens_distribution(generator);
		//random_sens = sens_distribution(generator);

		random_sens = (d * r1) + (r2 * sqrt(1 - (d * d)));

		if (r1 <= 0) { random_sens = -random_sens; }


		// Ensure outputted sensitivities are within the bounds set by the user
		if ((random_sens < MIN_SENS) || (random_sens > MAX_SENS))
		{
			// Iterate until a sensitivity within the range is achieved
			do {
				//normal_distribution<double> sens_distribution(SENS_MEAN, SENS_SPREAD);
				r1 = sens_distribution(generator);
				r2 = sens_distribution(generator);

				random_sens = (d * r1) + (r2 * sqrt(1 - (d * d)));
				if (r1 <= 0) { random_sens = -random_sens; };

			} while ((random_sens < MIN_SENS) || (random_sens > MAX_SENS));
		}

		// Update the mean sensitivity to be the current random sensitivity
		if ((random_sens < MIN_SENS) || (random_sens > MAX_SENS)) { SENS_MEAN = random_sens; }


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

		// Append to vectors of sensitivities and timesteps
		// Random timestep adds to previous timestep value to iteratively "move forward" in time
		x_vals.push_back(random_timestep + x_vals.back());
		y_vals.push_back(random_sens);
	}


	// Begin first of two steps to smooth the points

	// Generate a list of points to "connect" the random points
	// New point list linearly connects to each point after it
	vec xx = linspace<vec>(x_vals.front(), x_vals.back(), x_vals.size() * 200);
	vec yy;

	vec arma_x_vals(x_vals);
	vec arma_y_vals(y_vals);

	// Actual "interpolation" function to connect the points together
	arma::interp1(arma_x_vals, arma_y_vals, xx, yy);


	// Begin second step in smoothing

	// Generate gaussian window used in convolution
	vec n = linspace<vec>(0, GAUSSIAN_WINDOW_SIZE - 1, GAUSSIAN_WINDOW_SIZE) - (GAUSSIAN_WINDOW_SIZE - 1) / 2.0;
	double sig2 = 2 * GAUSSIAN_STDDEV * GAUSSIAN_STDDEV;
	vec w = exp(-square(n) / sig2);

	// Actual convolution call
	vec smooth_curve = conv(yy, w / sum(w), "same");

	// Convert back to std::vector to be used in Interception
	vector<double> timesteps = conv_to < vector<double> >::from(xx);
	vector<double> sensitivities = conv_to< vector<double> >::from(smooth_curve);

	// Chop off artifacts from smoothing algorithm
	// Smoothed curve is janky around beginning and ends
	vector<double> t(timesteps.begin() + 2500, timesteps.end() - 2500);
	vector<double> s(sensitivities.begin() + 2500, sensitivities.end() - 2500);

	double offset = t.front();
	for_each(t.begin(), t.end(), [offset](double& d) { d -= offset; });

	if (VISUALIZE)
	{
		ofstream myfile;
		myfile.open("sens_list.txt");
		myfile << MIN_SENS << "," << MAX_SENS << "\n";
		for (int i = 0; i < t.size(); i++) {
			myfile << t[i] << "," << s[i] << "\n";
		}
	}

	auto vals = make_tuple(t, s);

	return vals;
}


int main()
{
	InterceptionContext context;
	InterceptionDevice device;
	InterceptionStroke stroke;

	raise_process_priority();

	context = interception_create_context();

	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE);
	interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

	int i = 0;

	double
		dx,
		dy,
		passage,
		sens_multiplier,
		carryX = 0,
		carryY = 0;

	vector<double> x_vals, y_vals;

	tie(x_vals, y_vals) = generateSensitivities();
	
	int size = x_vals.size();

	typedef chrono::high_resolution_clock Time;
	typedef chrono::duration<float> fsec;

	auto prev_time = Time::now();
	auto curr_time = Time::now();

	auto old_prev = curr_time - prev_time;

	sens_multiplier = y_vals.front();

	static double percent;
	static bool paused = false;
	static DWORD lastpress = 0;

	std::printf("\nRunning...\n");

	while (interception_receive(context, device = interception_wait(context), &stroke, 1) > 0)
	{
		if (interception_is_mouse(device))
		{
			InterceptionMouseStroke& mstroke = *(InterceptionMouseStroke*)& stroke;

			if (!(mstroke.flags & INTERCEPTION_MOUSE_MOVE_ABSOLUTE)) {

				coord.X = 0;
				coord.Y = 21;

				fsec fs = curr_time - prev_time;
				passage = fs.count();

				if (i < x_vals.size())
				{
					if (passage > x_vals[i])
					{
						while (passage > x_vals[i]) { 
							if (i < x_vals.size()) { i++; }
							else { break; }
						}

						if (!paused) {
							sens_multiplier = y_vals[i];
							percent = double(i) / size * 100;
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

							if (paused) {
								coord.X = 20;
								coord.Y = 20;
								SetConsoleCursorPosition(hConsole, coord);
								SetConsoleTextAttribute(hConsole, 0x20);
								std::printf(" PAUSED ");
								SetConsoleTextAttribute(hConsole, 0x08);
							}
							else {
								coord.X = 20;
								coord.Y = 20;
								SetConsoleCursorPosition(hConsole, coord);
								std::printf("        ");
								SetConsoleTextAttribute(hConsole, 0x08);
							}
						}
						i++;
					}
				}
				else { break; }

				dx = mstroke.x * sens_multiplier + carryX;
				dy = mstroke.y * sens_multiplier + carryY;

				carryX = dx - floor(dx);
				carryY = dy - floor(dy);

				mstroke.x = (int)floor(dx);
				mstroke.y = (int)floor(dy);
			}

			interception_send(context, device, &stroke, 1);
			curr_time = Time::now();
		}

		if (interception_is_keyboard(device))
		{
			InterceptionKeyStroke& kstroke = *(InterceptionKeyStroke*)& stroke;

			interception_send(context, device, &stroke, 1);

			if (kstroke.code == SCANCODE_P && (GetTickCount64() - lastpress > 1000)) {
				lastpress = GetTickCount64();
				paused = !paused;
			}
		}
	}

	SetConsoleTextAttribute(hConsole, 0x02);
	std::printf("\n\n\n\n\nPlease restart the program to regenerate another smooth sensitivity curve.\n\n");
	SetConsoleTextAttribute(hConsole, 0x08);

	interception_destroy_context(context);

	return 0;
}
