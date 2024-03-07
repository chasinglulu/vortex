/* SPDX-License-Identifier: GPL-2.0
 *
 * Vortex AXI GPGPU SystemC TLM-2.0 Wrapper.
 *
 * Copyright (C) 2024, Charleye<wangkart@aliyun.com>
 * All rights reserved.
 *
 */

#include <iostream>
#include <systemc>
#include "VVortex_axi.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;

#ifndef TRACE_START_TIME
#define TRACE_START_TIME 0ull
#endif

#ifndef TRACE_STOP_TIME
#define TRACE_STOP_TIME -1ull
#endif

static uint64_t timestamp = 0;

double sc_time_stamp() {
	return timestamp;
}

static bool trace_enabled = false;
static uint64_t trace_start_time = TRACE_START_TIME;
static uint64_t trace_stop_time  = TRACE_STOP_TIME;

bool sim_trace_enabled() {
	if (timestamp >= trace_start_time
			&& timestamp < trace_stop_time)
		return true;

	return trace_enabled;
}

void sim_trace_enable(bool enable) {
	trace_enabled = enable;
}

SC_MODULE(Processor)
{
	SC_HAS_PROCESS(Processor);
	VVortex_axi *device;

	sc_signal<bool> rst, rst_n;
	sc_clock *clk;

	void gen_rst_n(void)
	{
		rst_n.write(!rst.read());
	}

	Processor(sc_module_name name) :
		rst("rst"),
		rst_n("rst_n")
	{
		SC_METHOD(gen_rst_n);
		sensitive << rst;

		/* Slow clock to keep simulation fast.  */
		clk = new sc_clock("clk", sc_time(10, SC_US));
		device = new VVortex_axi("gpgpu");

		device->clk(*clk);
		device->reset(rst_n);
	}
};

int sc_main(int argc, char **argv)
{
	Processor *vortex;

	Verilated::commandArgs(argc, argv);
	sc_set_time_resolution(1, SC_PS);

	vortex = new Processor("vortex");

	/* Pull the reset signal.  */
	vortex->rst.write(true);
	sc_start(1, SC_US);
	vortex->rst.write(false);

	sc_start();

	return 0;
}