// Copyright (c) 2025 radar_lamp project
// SPDX-License-Identifier: MIT

#include "led_dimmer.h"

#include <math.h>

#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// ── Hardware ────────────────────────────────────────────────────────────────
#define LED_GPIO         8
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_DUTY_BITS   LEDC_TIMER_10_BIT   // 0–1023
#define LEDC_FREQ_HZ     5000

// ── Distance → brightness mapping ───────────────────────────────────────────
#define DIST_FULL_BRIGHT_M  0.5f   // ≤ 0.5 m  → 100 %
// DIST_FULL_DIM_M is defined in led_dimmer.h (shared with main.c)

// ── Smoothing & timing ───────────────────────────────────────────────────────
#define TICK_MS          10     // LED task period
#define TRACK_STEP       2.0f   // max % change per tick while tracking  (~0.5 s full sweep)
#define FADE_DECAY       0.96f  // exponential fade multiplier per tick   (~1.7 s to black)
#define FADE_DECAY_SLOW  0.985f // slower decay for the last portion of fade
#define FADE_SLOW_THRESH 15.0f  // % below which we switch to the slower decay
#define HOLD_TIMEOUT_MS  5000   // ms with no reading before fading starts (extra buffer for slow steps)

// ── State machine ────────────────────────────────────────────────────────────
typedef enum
{
	STATE_TRACKING, // valid readings coming in, follow distance
	STATE_HOLDING,  // no reading yet, hold brightness, start countdown
	STATE_FADING,   // countdown expired, slowly go to 0
} led_state_t;

// ── Shared state (protected by mutex) ────────────────────────────────────────
static SemaphoreHandle_t s_mutex;
static float             s_target_brightness = 0.0f;
static bool              s_reading_valid     = false;
static int64_t           s_last_valid_ms     = 0;
static int               s_invalid_count     = 0;

#define INVALID_THRESHOLD 6  // consecutive invalid readings before going to HOLDING

// ── Distance smoothing pipeline ───────────────────────────────────────────────
// Stage 1 – median filter (3-sample): rejects isolated spike readings.
// Stage 2 – adaptive EMA: alpha scales with the speed of distance change so
//   fast movement is followed immediately while slow steps are smoothed out.

#define MEDIAN_WIN   3
#define ALPHA_MIN    0.15f  // heavy smoothing when nearly stationary
#define ALPHA_SCALE  3.0f   // how fast alpha grows with movement speed

static float s_dist_buf[MEDIAN_WIN] = {0};
static int   s_dist_idx             = 0;
static float s_ema_dist             = -1.0f;  // negative = uninitialized

static float median3(float a, float b, float c)
{
	return a + b + c - fmaxf(a, fmaxf(b, c)) - fminf(a, fminf(b, c));
}

static float distance_smooth(float dist_m)
{
	// Stage 1: median
	s_dist_buf[s_dist_idx] = dist_m;
	s_dist_idx             = (s_dist_idx + 1) % MEDIAN_WIN;
	float median = median3(s_dist_buf[0], s_dist_buf[1], s_dist_buf[2]);

	// Stage 2: adaptive EMA
	if (s_ema_dist < 0.0f)
		s_ema_dist = median;  // first call — seed with current value

	float delta    = fabsf(median - s_ema_dist);
	float alpha    = fminf(1.0f, ALPHA_MIN + delta * ALPHA_SCALE);
	s_ema_dist     = alpha * median + (1.0f - alpha) * s_ema_dist;

	return s_ema_dist;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

// Gamma correction: human eyes perceive brightness logarithmically.
// Applying powf(x, 2.2) makes the LED appear to change evenly with distance.
#define GAMMA 2.2f

static void ledc_set_brightness(float pct)
{
	float linear = pct / 100.0f;
	float corrected = powf(linear, GAMMA);
	uint32_t duty = (uint32_t)(corrected * 1023.0f);
	ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
	ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static float distance_to_brightness(float dist_m)
{
	if (dist_m <= DIST_FULL_BRIGHT_M)
		return 100.0f;
	if (dist_m >= DIST_FULL_DIM_M)
		return 0.0f;
	return (DIST_FULL_DIM_M - dist_m) / (DIST_FULL_DIM_M - DIST_FULL_BRIGHT_M) * 100.0f;
}

static float step_toward(float current, float target, float max_step)
{
	float diff = target - current;
	if (diff > max_step)
		return current + max_step;
	if (diff < -max_step)
		return current - max_step;
	return target;
}

// ── LED task ─────────────────────────────────────────────────────────────────

static void led_task(void *arg)
{
	led_state_t state             = STATE_HOLDING;
	float       current_brightness = 0.0f;

	// Initialise last_valid to now so HOLD_TIMEOUT doesn't fire immediately
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	s_last_valid_ms = esp_timer_get_time() / 1000;
	xSemaphoreGive(s_mutex);

	while (true)
	{
		// Snapshot shared state
		xSemaphoreTake(s_mutex, portMAX_DELAY);
		bool    valid   = s_reading_valid;
		float   target  = s_target_brightness;
		int64_t elapsed = (esp_timer_get_time() / 1000) - s_last_valid_ms;
		xSemaphoreGive(s_mutex);

		switch (state)
		{
			case STATE_TRACKING:
				if (!valid)
				{
					state = STATE_HOLDING;
				}
				else
				{
					current_brightness = step_toward(current_brightness, target, TRACK_STEP);
					ledc_set_brightness(current_brightness);
				}
				break;

			case STATE_HOLDING:
				if (valid)
				{
					state = STATE_TRACKING;
				}
				else if (elapsed > HOLD_TIMEOUT_MS)
				{
					state = STATE_FADING;
				}
				// brightness unchanged while holding
				break;

			case STATE_FADING:
				if (valid)
				{
					state = STATE_TRACKING;
				}
				else
				{
					float decay = (current_brightness < FADE_SLOW_THRESH) ? FADE_DECAY_SLOW : FADE_DECAY;
					current_brightness *= decay;
					if (current_brightness < 0.1f)
						current_brightness = 0.0f;
					ledc_set_brightness(current_brightness);
				}
				break;
		}

		vTaskDelay(pdMS_TO_TICKS(TICK_MS));
	}
}

// ── Public API ───────────────────────────────────────────────────────────────

void led_dimmer_init(void)
{
	s_mutex = xSemaphoreCreateMutex();

	ledc_timer_config_t timer_cfg = {
		.speed_mode      = LEDC_MODE,
		.timer_num       = LEDC_TIMER,
		.duty_resolution = LEDC_DUTY_BITS,
		.freq_hz         = LEDC_FREQ_HZ,
		.clk_cfg         = LEDC_AUTO_CLK,
	};
	ledc_timer_config(&timer_cfg);

	ledc_channel_config_t channel_cfg = {
		.gpio_num   = LED_GPIO,
		.speed_mode = LEDC_MODE,
		.channel    = LEDC_CHANNEL,
		.timer_sel  = LEDC_TIMER,
		.duty       = 0,
		.hpoint     = 0,
	};
	ledc_channel_config(&channel_cfg);

	xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}

void led_dimmer_update(float distance_m, bool valid)
{
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	if (valid)
	{
		s_invalid_count     = 0;
		s_reading_valid     = true;
		s_target_brightness = distance_to_brightness(distance_smooth(distance_m));
		s_last_valid_ms     = esp_timer_get_time() / 1000;
	}
	else
	{
		s_invalid_count++;
		if (s_invalid_count >= INVALID_THRESHOLD)
			s_reading_valid = false;
	}
	xSemaphoreGive(s_mutex);
}
