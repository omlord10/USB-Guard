//Copyright (C)2014-2024 GOWIN Semiconductor Corporation.
//All rights reserved.
//File Title: Timing Constraints file
create_clock -name clk_osc -period 37.037 -waveform {0 18.518} [get_ports {sys_clk}]
