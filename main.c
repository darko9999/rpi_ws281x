/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "math.h"

#include "ws2811.h"

#define ARRAY_SIZE(stuff) (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ WS2811_TARGET_FREQ
#define GPIO_PIN 12
#define DMA 10
#define STRIP_TYPE WS2811_STRIP_RGB // WS2812/SK6812RGB integrated chip+leds
// #define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
// #define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT 120

int led_count = LED_COUNT;

int clear_on_exit = 1;

ws2811_t ledstring =
	{
		.freq = TARGET_FREQ,
		.dmanum = DMA,
		.channel =
			{
				[0] =
					{
						.gpionum = GPIO_PIN,
						.invert = 0,
						.count = LED_COUNT,
						.strip_type = STRIP_TYPE,
						.brightness = 255,
					},
				[1] =
					{
						.gpionum = 0,
						.invert = 0,
						.count = 0,
						.brightness = 0,
					},
			},
};

ws2811_led_t *line;

static uint8_t running = 1;

void line_render(void)
{
	for (int i = 0; i < led_count; i++)
	{
		ledstring.channel[0].leds[i] = line[i];
	}
}

// 0x00200000, // red
// 0x00201000, // orange
// 0x00202000, // yellow
// 0x00002000, // green
// 0x00002020, // lightblue
// 0x00000020, // blue
// 0x00100010, // purple
// 0x00200010, // pink
// 0x00200000, // red
// 0x10200000, // red + W
// 0x00002000, // green
// 0x10002000, // green + W
// 0x00000020, // blue
// 0x10000020, // blue + W
// 0x00101010, // white
// 0x10101010, // white + W

int letters[7][35] = {
	{1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0},
	{1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0},
	{1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0},
	{1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
	{1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0},
	{1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0},
};

int current_column = 0;

uint8_t r = 255;
uint8_t g = 3;
uint8_t b = 214;

int fps = 60;
int orientation = -1;
float intensity = 1.0;
const int nombre_pas = 360;
int min_pas = 1;
int max_pas = nombre_pas;
int pas = nombre_pas;

const float pi = 3.14159265358979323846;
const float deux_pi = pi * 2.0;
const float tier_de_tour = 1.0 / 3.0 * deux_pi;

void line_create_color(void)
{
	r = (line[0] >> 16) & 0xff;
	g = (line[0] >> 8) & 0xff;
	b = (line[0]) & 0xff;

	pas = pas + orientation;
	if (pas <= min_pas)
	{
		orientation = -orientation;
	}
	if (pas >= max_pas)
	{
		orientation = -orientation;
	}

	intensity = (float)pas / (float)max_pas;

	r = 255.0 * intensity;
	g = 3.0 * intensity;
	b = 214.0 * intensity;

	r = 255.0 * sin(intensity * deux_pi);
	g = 255.0 * sin(intensity * deux_pi + tier_de_tour);
	b = 255.0 * sin(intensity * deux_pi + 2.0 * tier_de_tour);

	// for (int j = 0; j < 7; j++)
	// {
	// 	line[17 * j] = letters[j][current_row_start] ? 0xffffff : 0;
	// }
	// current_column++;

	// printf("%2x %2x %2x\n", r, g, b);

	line[0] = (r << 16) + (g << 8) + b;

	// printf("%8x\n", line[0]);
}

void line_animate(void)
{
	for (int i = 1; i < led_count; i++)
	{
		line[led_count - i] = line[led_count - i - 1];
	}
	// for (int column = 0; column < 17; column++)
	// {
	// 	for (int row = 0; row < 7; row++)
	// 	{
	// 		int columnOffset = (current_column + column) % 35;
	// 		int letter = letters[row][columnOffset];
	// 		int lineIndex = column + row * 17;
	// 		int oddRow = row % 2 == 1;
	// 		if (oddRow)
	// 			lineIndex = (17 - column) + row * 17;
	// 		line[lineIndex] = letter ? 0xFF00BF : 0;
	// 	}
	// }
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
	running = 0;
}

static void setup_handlers(void)
{
	struct sigaction sa =
		{
			.sa_handler = ctrl_c_handler,
		};

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

int main(int argc, char *argv[])
{
	printf("starting!\n");

	ws2811_return_t ret;

	line = malloc(sizeof(ws2811_led_t) * led_count);

	setup_handlers();

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
	{
		fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
		return ret;
	}

	while (running)
	{
		line_animate();
		line_create_color();
		line_render();

		if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
		{
			fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
			break;
		}

		usleep(1000000 / fps);
	}

	for (int i = 0; i < led_count; i++)
	{
		line[i] = 0;
	}
	line_render();
	ws2811_render(&ledstring);

	ws2811_fini(&ledstring);

	printf("\n");
	return ret;
}
