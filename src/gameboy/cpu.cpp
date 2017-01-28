void CPU::reset() {
	cycle_count = 0;
	halted = false;
	state = CPU_STATE_FETCH;
	address = 0;
	bus = 0;
	instruction = nullptr;
	condition = false;

	AF = 0;
	BC = 0;
	DE = 0;
	HL = 0;
	SP = 0;
	PC = 0;

	DEBUG_not_implemented_error = false;
}

void CPU::step() {
	// update timers
	// CPU_HZ = 1<<22; 4 MiHz
	// DIV_HZ = 1<<14; 16 KiH
	if ((cycle_count&0xFF) == 0)  memory->io.DIV++;

	if (memory->io.TAC_stop == 1) { // timer running
		int period = 0;
		switch (memory->io.TAC_clock) {
			case 0: period = 1<<10; break; // 1<<12 Hz
			case 1: period = 1<<4; break; // 1<<18 Hz
			case 2: period = 1<<6; break; // 1<<16 Hz
			case 3: period = 1<<8; break; // 1<<14 Hz
		}
		if ((cycle_count&(period-1)) == 0) {
			if (memory->io.TIMA == 0xFF) {
				memory->io.TIMA = memory->io.TMA; // reset
				memory->io.IF_timer = 1; // raise interrupt
				halted = false;
			} else {
				memory->io.TIMA++;
			}
		}
	}

	cycle_count++;

	CPUState old_state = state;
	state = CPU_STATE_IDLE0;

	switch (old_state) {
	case CPU_STATE_FETCH:
		// TODO: handle more IRQs
		if (IME) {
			if (memory->io.IE_vblank && memory->io.IF_vblank) {
				IME = false;
				halted = false;
				address = IRQ_ADR_VBLANK;
				instruction = &CPU::irq;
				memory->io.IF_vblank = 0;
				break;
			} else if (memory->io.IE_lcd_stat && memory->io.IF_lcd_stat) {
				IME = false;
				halted = false;
				address = IRQ_ADR_LCDSTAT;
				instruction = &CPU::irq;
				memory->io.IF_lcd_stat = 0;
				break;
			} else if (memory->io.IE_timer && memory->io.IF_timer) {
				IME = false;
				halted = false;
				address = IRQ_ADR_TIMER;
				instruction = &CPU::irq;
				memory->io.IF_timer = 0;
				break;
			}
		}
		if (!halted) {
			bus = memory->load8(PC++);
			instruction = instructions[bus];
		}
		break;
	case CPU_STATE_MEMORY_LOAD:
		bus = memory->load8(address);
		break;
	case CPU_STATE_MEMORY_STORE:
		memory->store8(address, bus);
		break;
	case CPU_STATE_READ_PC:
		bus = memory->load8(PC++);
		break;
	case CPU_STATE_STALL:
		instruction = &CPU::nop;
		break;
	default: break;
	}

	cycle_count += 2;

	state = CPU_STATE_FETCH;
	if (instruction == nullptr) {
		LOGE("unimplemented instruction 0x%02X %s",
			bus, instruction_infos[bus].mnemonic);
		DEBUG_not_implemented_error = true;
		return;
	}
	(this->*instruction)();
	
	cycle_count++;
}
