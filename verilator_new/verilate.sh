# verilator -cc -exe --public --compiler msvc --converge-limit 2000 -Wno-WIDTH -Wno-IMPLICIT -Wno-MODDUP -Wno-UNSIGNED -Wno-CASEINCOMPLETE -Wno-CASEX -Wno-SYMRSVDWORD -Wno-COMBDLY -Wno-INITIALDLY -Wno-BLKANDNBLK -Wno-UNOPTFLAT -Wno-SELRANGE -Wno-CMPCONST -Wno-CASEOVERLAP   -Wno-PINMISSING --top-module top centipede_sim.v ../rtl/centipede.v ../rtl/p6502.v ../rtl/pokey.v ../rtl/ram.v ../rtl/rom.v ../rtl/color_ram.v ../rtl/pf_rom.v ../rtl/pf_ram_dp.v ../rtl/vprom.v ../rtl/hs_ram.v

verilator -cc -exe --public --compiler msvc +define+SIMULATION=1  --converge-limit 2000 -Wno-UNOPTFLAT --top-module top centipede_sim.v ../rtl/centipede.v ../rtl/p6502.v ../rtl/pokey.v ../rtl/ram.v ../rtl/rom.v ../rtl/color_ram.v ../rtl/pf_rom.v ../rtl/pf_ram_dp.v ../rtl/vprom.v ../rtl/hs_ram.v