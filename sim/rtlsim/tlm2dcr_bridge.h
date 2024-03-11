/*
 * TLM-2.0 to Vortex DCR interface.
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

class tlm2dcr_bridge: public sc_core::sc_module
{
public:
	tlm_utils::simple_target_socket<tlm2dcr_bridge> tgt_socket;

	tlm2dcr_bridge(sc_core::sc_module_name name);
	SC_HAS_PROCESS(tlm2dcr_bridge);

	sc_in<bool> clk;
	sc_out<bool> dcr_wr_valid;
	sc_out<sc_bv<12> > dcr_wr_addr;
	sc_out<sc_bv<32> > dcr_wr_data;

private:
	virtual void b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay);
};

tlm2dcr_bridge::tlm2dcr_bridge(sc_module_name name)
	: sc_module(name), tgt_socket("tgt_socket"),
	clk("clk"),
	dcr_wr_valid("dcr_wr_valid"),
	dcr_wr_addr("dcr_wr_addr"),
	dcr_wr_data("dcr_wr_data")
{
	tgt_socket.register_b_transport(this, &tlm2dcr_bridge::b_transport);
}

void tlm2dcr_bridge::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64    addr = trans.get_address();
	unsigned char*   data = trans.get_data_ptr();
	unsigned int     len = trans.get_data_length();
	unsigned char*   byt = trans.get_byte_enable_ptr();
	unsigned int     wid = trans.get_streaming_width();
	uint32_t dcr_data = 0;
	uint32_t tpwdata = 0;

	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}

	if (len != 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	/* Because we wait for events we need to accomodate delay.  */
	wait(delay, clk.negedge_event());
	delay = SC_ZERO_TIME;

	if (cmd == tlm::TLM_WRITE_COMMAND) {
		dcr_wr_valid.write(true);
		//memcpy(&dcr_wr_data, data, 32);
		dcr_wr_addr.write(addr);
	}

	do {
		wait(clk.posedge_event());

		dcr_wr_valid.write(false);
	} while (dcr_wr_valid.read() == true);

	trans.set_response_status(tlm::TLM_OK_RESPONSE);
}