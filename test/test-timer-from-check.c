/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

static uv_prepare_t prepare_handle;
static uv_check_t check_handle;
static uv_timer_t timer_handle;

static int prepare_cb_called;
static int check_cb_called;
static int timer_cb_called;


static void prepare_cb(uv_prepare_t* handle) {
  ASSERT_OK(uv_prepare_stop(&prepare_handle));
  ASSERT_OK(prepare_cb_called);
  ASSERT_EQ(1, check_cb_called);
  ASSERT_OK(timer_cb_called);
  prepare_cb_called++;
}


static void timer_cb(uv_timer_t* handle) {
  ASSERT_OK(uv_timer_stop(&timer_handle));
  ASSERT_EQ(1, prepare_cb_called);
  ASSERT_EQ(1, check_cb_called);
  ASSERT_OK(timer_cb_called);
  timer_cb_called++;
}


static void check_cb(uv_check_t* handle) {
  ASSERT_OK(uv_check_stop(&check_handle));
  ASSERT_OK(uv_timer_stop(&timer_handle));  /* Runs before timer_cb. */
  ASSERT_OK(uv_timer_start(&timer_handle, timer_cb, 50, 0));
  ASSERT_OK(uv_prepare_start(&prepare_handle, prepare_cb));
  ASSERT_OK(prepare_cb_called);
  ASSERT_OK(check_cb_called);
  ASSERT_OK(timer_cb_called);
  check_cb_called++;
}


TEST_IMPL(timer_from_check) {
  ASSERT_OK(uv_prepare_init(uv_default_loop(), &prepare_handle));
  ASSERT_OK(uv_check_init(uv_default_loop(), &check_handle));
  ASSERT_OK(uv_check_start(&check_handle, check_cb));
  ASSERT_OK(uv_timer_init(uv_default_loop(), &timer_handle));
  ASSERT_OK(uv_timer_start(&timer_handle, timer_cb, 50, 0));
  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_EQ(1, prepare_cb_called);
  ASSERT_EQ(1, check_cb_called);
  ASSERT_EQ(1, timer_cb_called);
  uv_close((uv_handle_t*) &prepare_handle, NULL);
  uv_close((uv_handle_t*) &check_handle, NULL);
  uv_close((uv_handle_t*) &timer_handle, NULL);
  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_ONCE));
  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}


/* A ref'd idle handle started from an unref'd check handle keeps the loop
 * alive. A timer that runs earlier in the same iteration creates the work,
 * so the check handle must observe it and start the idle handle. */
static uv_idle_t keepalive_idle;
static uv_check_t keepalive_check;
static uv_timer_t keepalive_timer;
static int keepalive_work;
static int keepalive_done;
static int keepalive_idle_cb_called;
static int keepalive_timer_cb_called;


static void keepalive_idle_cb(uv_idle_t* handle) {
  keepalive_idle_cb_called++;
  keepalive_done = 1;
  ASSERT_OK(uv_idle_stop(handle));
}


static void keepalive_check_cb(uv_check_t* handle) {
  if (keepalive_work && !keepalive_done)
    ASSERT_OK(uv_idle_start(&keepalive_idle, keepalive_idle_cb));
}


static void keepalive_timer_cb(uv_timer_t* handle) {
  keepalive_timer_cb_called++;
  keepalive_work = 1;
}


static void keepalive_setup(void) {
  keepalive_work = 0;
  keepalive_done = 0;
  keepalive_idle_cb_called = 0;
  keepalive_timer_cb_called = 0;
  ASSERT_OK(uv_idle_init(uv_default_loop(), &keepalive_idle));
  ASSERT_OK(uv_check_init(uv_default_loop(), &keepalive_check));
  ASSERT_OK(uv_check_start(&keepalive_check, keepalive_check_cb));
  uv_unref((uv_handle_t*) &keepalive_check);
  ASSERT_OK(uv_timer_init(uv_default_loop(), &keepalive_timer));
  ASSERT_OK(uv_timer_start(&keepalive_timer, keepalive_timer_cb, 10, 0));
}


static void keepalive_teardown(void) {
  uv_close((uv_handle_t*) &keepalive_idle, NULL);
  uv_close((uv_handle_t*) &keepalive_check, NULL);
  uv_close((uv_handle_t*) &keepalive_timer, NULL);
  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_ONCE));
  MAKE_VALGRIND_HAPPY(uv_default_loop());
}


TEST_IMPL(timer_before_check_keepalive) {
  keepalive_setup();
  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_EQ(1, keepalive_timer_cb_called);
  ASSERT_EQ(1, keepalive_idle_cb_called);
  keepalive_teardown();
  return 0;
}
