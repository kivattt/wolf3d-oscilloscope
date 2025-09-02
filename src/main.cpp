#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <mutex>
#include <iostream>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdint>
#include <cstring>

#define SAMPLERATE 192000
#define BRUH 8192 // 8192 is the largest we can get away with at 192khz without oscilloscope flicker (max hold pot)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

std::mutex mu;
std::int16_t *currentFrame;

// Remember to XDestroyImage() the returned pointer
XImage *screenshot() {
	Display* display = XOpenDisplay(nullptr);
	Window root = DefaultRootWindow(display);

	XWindowAttributes attributes = {0};
	XGetWindowAttributes(display, root, &attributes);

	int Width = attributes.width;
	int Height = attributes.height;

	XImage* img = XGetImage(display, root, 0, 0 , Width, Height, AllPlanes, ZPixmap);
	XCloseDisplay(display);
	return img;
}

static unsigned int g_seed;

// Used to seed the generator.           
inline void fast_srand(int seed) {
    g_seed = seed;
}

inline int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

class MyAudio : public sf::SoundStream {
	bool onGetData(Chunk &data) {
		mu.lock();
		data.samples = currentFrame;
		data.sampleCount = BRUH;
		mu.unlock();
		return true;
	}

	void onSeek(sf::Time timeOffset) {
		return;
	}

	public:
	void load() {
		initialize(2, SAMPLERATE, {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight});
	}
};

float lerp(float a, float b, float t) {
	return a * (1 - t) + b * t;
}

void populate(std::int16_t *samples, std::size_t count, std::int16_t *tallLUT, std::int16_t *shortLUT, float *wallHeights, int numWallHeights) {
	// Zero-out the sample buffer
	for (int i = 0; i < BRUH; i++) {
		samples[i] = 0.0;
	}

	int outputStart = 400;
	int outputEnd = 5000;
	int outputRange = outputEnd - outputStart;

	for (int i = outputStart; i < outputRange; i++) {
		int indexWall = double(i) / double(outputRange) * double(numWallHeights);
		float height = wallHeights[indexWall];
		float output = tallLUT[i] * height;
		samples[i] = 0.15 * output;
	}

	// Negative trigger signal
	const int N = 16;
	for (int i = 0; i < N; i++) {
		samples[i] = -32767;
	}
}

void generate_LUT(std::int16_t *lut, int count, bool tall = false) {
	float amplitude = tall ? 1.0 : 0.05;
	float frequency = tall ? 600.0 : 50.0;

	for (int i = 0; i < count; i++) {
		lut[i] = 8192.0 * amplitude * sin(frequency * double(i) / double(count) * M_PI);
	}
}

int xy_to_index(int x, int y, int width) {
	return x + y * width;
}

// Screenshots and produces the wall heights
void get_wall_heights_from_screenshot(float *wallHeights, int numWallHeights) {
	int offsetX    = 720;
	int offsetY    = 412;
	int gameWidth  = 480;
	int gameHeight = 240;

	int heightStep = 3;

	XImage *img = screenshot();
	int sizeInBytes = img->width * img->height * (img->bits_per_pixel / 8);

	const unsigned int CEILING_COLOR = 56;
	const unsigned int FLOOR_COLOR = 113;
	for (int i = 0; i < numWallHeights; i++) {
		int x = offsetX + i * (gameWidth / numWallHeights);
		const int SCALE = 6;

		// Look for ceiling from the top
		int heightCount = gameHeight / 2 / heightStep - SCALE;
		for (int y = offsetY; y < offsetY + gameHeight/2; y += heightStep) {
			int index = xy_to_index(x, y, img->width);
			unsigned char r = img->data[4 * index + 0];
			unsigned char g = img->data[4 * index + 1];
			unsigned char b = img->data[4 * index + 2];

			// If it isn't grayscale, stop the search
			if (r != g || g != b || r != b) {
				break;
			}

			// If it isn't a ceiling, stop the search
			if (r != CEILING_COLOR) {
				break;
			}

			heightCount -= 1;
		}

		// Look for floor from the bottom
		int heightCount2 = gameHeight / 2 / heightStep - SCALE;
		for (int y = offsetY + gameHeight - 1; y > offsetY + gameHeight / 2; y -= heightStep) {
			int index = xy_to_index(x, y, img->width);
			unsigned char r = img->data[4 * index + 0];
			unsigned char g = img->data[4 * index + 1];
			unsigned char b = img->data[4 * index + 2];

			// If it isn't grayscale, stop the search
			if (r != g || g != b || r != b) {
				break;
			}

			// If it isn't a ceiling, stop the search
			if (r != FLOOR_COLOR) {
				break;
			}

			heightCount2 -= 1;
		}

		// Use the best result
		heightCount = MIN(heightCount, heightCount2);

		wallHeights[i] = double(heightCount) / (double(gameHeight) / 2.0 / double(heightStep));
		//std::cout << "wall height at i=" << i << " is " << wallHeights[i] << '\n';
	}

	XDestroyImage(img);
}

int main() {
	std::srand(std::time(nullptr));

	mu.lock();
	currentFrame = (std::int16_t*)malloc(BRUH * 2);
	mu.unlock();

	auto window = sf::RenderWindow(sf::VideoMode({1920u, 1080u}), "Game");
	window.setFramerateLimit(100);

	// WALL HEIGHTS
	const int NUM_WALL_HEIGHTS = 200;
	float wallHeights[NUM_WALL_HEIGHTS]; // 0.0 to 1.0
	for (int i = 0; i < NUM_WALL_HEIGHTS; i++) {
		wallHeights[i] = 0.0;
	}

	// Tall and short LUTs
	std::int16_t *tallLUT = (std::int16_t*)malloc(BRUH * 2);
	std::int16_t *shortLUT = (std::int16_t*)malloc(BRUH * 2);
	generate_LUT(tallLUT, BRUH, true);
	generate_LUT(shortLUT, BRUH, false);

	MyAudio myAudio;
	myAudio.load();
	myAudio.play();
	while (window.isOpen()) {
		while (const std::optional event = window.pollEvent()) {
			if (event->is<sf::Event::Closed>()) {
				window.close();
			}
		}

		get_wall_heights_from_screenshot(wallHeights, NUM_WALL_HEIGHTS);

		mu.lock();
		populate(currentFrame, BRUH, tallLUT, shortLUT, wallHeights, NUM_WALL_HEIGHTS);
		mu.unlock();

		window.clear();
		window.display();
	}

	mu.lock();
	free(currentFrame);
	mu.unlock();

	free(tallLUT);
	free(shortLUT);

	return 0;
}
