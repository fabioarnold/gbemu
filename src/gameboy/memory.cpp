void Memory::init() {
	assert(sizeof(IO) == 0x100);
	reset();

	if (rom) { delete [] rom; }
	rom = nullptr;
	rom_size = 0;
	rom_bank0 = nullptr;
	rom_bank1 = nullptr;

	if (sram) { delete [] sram; }
	sram = nullptr;
	sram_size = 0;
	sram_bank = nullptr;

	mbc = &Memory::mbc0;
}

void Memory::reset() {
	memset(vram, 0, sizeof(vram));
	memset(ram,  0, sizeof(ram));
	memset(&oam, 0, sizeof(oam));
	memset(&io,  0, sizeof(io));
	memset(hram, 0, sizeof(hram));

	if (rom) {
		rom_bank0 = rom;
		rom_bank1 = rom+SIZE_ROM_BANK;
	}

	if (sram) {
		sram_bank = sram;
	}

	sram_enabled = false;
}

u8 Memory::load8(u16 address) {
	if ((address >= ADR_IO && address < ADR_IO+SIZE_IO) || address == ADR_IE) {
		gb->onIORead(address - ADR_IO);
	}
	if (address >= gb->apu.start_addr && address <= gb->apu.end_addr) {
		u64 frame_cycle_count = gb->cpu.cycle_count - gb->frame_begin_cycle_count;
		return gb->apu.read_register(frame_cycle_count, address);
	}
	return *map(address);
}

void Memory::store8(u16 address, u8 value) {
	if (address < ADR_ROM_BANK0 + 2*SIZE_ROM_BANK) { // ROM
		(this->*mbc)(address, value);
	} else {
		if ((address >= ADR_IO && address < ADR_IO+SIZE_IO) || address == ADR_IE) {
			value = gb->onIOWrite(address - ADR_IO, value);
		}
		if (address >= gb->apu.start_addr && address <= gb->apu.end_addr) {
			u64 frame_cycle_count = gb->cpu.cycle_count - gb->frame_begin_cycle_count;
			gb->apu.write_register(frame_cycle_count, address, (int)value);
		}
		*map(address) = value;
	}
}

void Memory::setROMBank(u8 bank) {
	assert(bank * SIZE_ROM_BANK < rom_size);
	rom_bank1 = &rom[bank * SIZE_ROM_BANK];
}

void Memory::setSRAMBank(u8 bank) {
	if (!sram_size) {
		LOGW("trying to set SRAM bank but no SRAM installed");
		return;
	}
	assert(bank * SIZE_RAM < sram_size);
	sram_bank = &sram[bank * SIZE_RAM]; // SIZE_RAM_BANK
}

void Memory::mbc0(u16 address, u8 value) {
	switch (address>>13) {
	case 0x1: // 0x2000 - 0x3FFF
		setROMBank(1);
		break;
	default:
		LOGW("attempt to write 0x%02X to ROM at 0x%04X", value, address);
	}
}

struct MBC1State {
	u8 lbank : 5;
	u8 hbank : 2; // bit 5 and 6 used depending on mode
	u8 mode  : 1; // 0: ROM banking, 1: RAM banking
} mbc1_state = {0};

void Memory::mbc1(u16 address, u8 value) {
	switch (address>>13) {
	case 0x0: // 0x0000 - 0x1FFF enable/disable SRAM
		// 4 lower bits are 0: disable SRAM, 0xA: enable SRAM
		sram_enabled = (value&0xF) == 0xA;
		break;
	case 0x1: // 0x2000 - 0x3FFF switch ROM bank 1
		mbc1_state.lbank = value&0x1F;
		if (!mbc1_state.lbank) mbc1_state.lbank++;
		if (mbc1_state.mode) { // RAM
			setROMBank(mbc1_state.lbank);
		} else { // ROM
			setROMBank((mbc1_state.hbank<<5) | mbc1_state.lbank);
		}
		if (!(value&0x1F)) value++;
		break;
	case 0x2: // 0x4000 - 0x5FFF switch SRAM bank/high ROM bank
		mbc1_state.hbank = value&3;
		if (mbc1_state.mode) { // RAM
			setSRAMBank(mbc1_state.hbank);
		} else { // ROM
			setROMBank((mbc1_state.hbank<<5) | mbc1_state.lbank);
		}
		break;
	case 0x3: // 0x6000 - 0x7FFF ROM/RAM mode select
		mbc1_state.mode = value&1;
		if (mbc1_state.mode) { // RAM banking mode
			setROMBank(mbc1_state.lbank);
		} else { // ROM banking mode
			setSRAMBank(0);
		}
		break;
	}
}

void Memory::mbc2(u16 address, u8 value) {
	switch (address>>13) {
	case 0x0:
		// least significant bit of the upper address byte must be zero to enable/disable cart SRAM
		sram_enabled = (value&0xF) == 0xA;
		break;
	case 0x1:
		// least significant bit of the upper address byte must be one to select a ROM bank
		{
			u8 bank = value&0xF;
			if (!bank) bank++;
			setROMBank(bank);
		}
		break;
	default: break;
		//LOGW("MBC2 unknown address 0x%04X", address);
	}
}

void Memory::mbc3(u16 address, u8 value) {
	switch (address>>13) {
	case 0x0: // 0x0000 - 0x1FFF enable/disable SRAM
		// 4 lower bits are 0: disable SRAM, 0xA: enable SRAM
		sram_enabled = (value&0xF) == 0xA;
		break;
	case 0x1: // 0x2000 - 0x3FFF switch ROM bank 1
		if (!value) value = 1;
		setROMBank(value);
		break;
	case 0x2: // 0x4000 - 0x5FFF switch SRAM bank
		if (value < 4) {
			setSRAMBank(value);
		} // else RTC
		break;
	case 0x3: // 0x6000 - 0x7FFF ROM/RAM mode
		break;
	}
}

void Memory::DMAtoOAM(u16 src_address) {
	// instant DMA transfer TODO: do this within 160 cycles
	assert(sizeof(oam) == SIZE_OAM);
	u8 *src = map(src_address);
	memcpy((void*)&oam, src, SIZE_OAM);
}

u8 *Memory::map(u16 address) {
	if (address < sizeof(boot_rom) && !io.BOOT) {
		return &boot_rom[address];
	} else if (address >= ADR_ROM_BANK0
		    && address <  ADR_ROM_BANK0 + SIZE_ROM_BANK) {
		return &rom_bank0[address - ADR_ROM_BANK0];
	} else if (address >= ADR_ROM_BANK1
		    && address <  ADR_ROM_BANK1 + SIZE_ROM_BANK) {
		return &rom_bank1[address - ADR_ROM_BANK1];
	} else if (address >= ADR_VRAM
		    && address <  ADR_VRAM + SIZE_VRAM) {
		return &vram[address - ADR_VRAM];
	} else if (address >= ADR_RAM_EXTERNAL
		    && address <  ADR_RAM_EXTERNAL + SIZE_RAM) {
		if (!sram_size) { // TODO: exception for MBC2
			static u8 null_byte = 0;
			//LOGW("accessing SRAM but no SRAM installed @ 0x%04X", address);
			return &null_byte;
		}
		assert(address - ADR_RAM_EXTERNAL < sram_size);
		return &sram_bank[address - ADR_RAM_EXTERNAL];
	} else if (address >= ADR_RAM_INTERNAL_BANK0
		    && address <  ADR_RAM_INTERNAL_BANK0 + SIZE_RAM) {
		return &ram[address - ADR_RAM_INTERNAL_BANK0];
	} else if (address >= ADR_RAM_INTERNAL_MIRROR
		    && address <  ADR_OAM) { // size != SIZE_RAM
		LOGI("accessing RAM (echo) at 0x%04X", address);
		return &ram[address - ADR_RAM_INTERNAL_MIRROR];
	} else if (address >= ADR_OAM
		    && address < ADR_OAM + SIZE_OAM) {
		return &((u8*)&oam)[address - ADR_OAM];
	} else if (address >= ADR_EMPTY && address < ADR_IO) {
		static u8 empty = 0;
		return &empty;
	} else if (address >= ADR_IO && address < ADR_HRAM) {
		return &((u8*)&io)[address - ADR_IO];
	} else if (address >= ADR_HRAM
		    && address <  ADR_HRAM + SIZE_HRAM) {
		return &hram[address - ADR_HRAM];
	} else if (address == ADR_IE) {
		return &io.IE;
	} else {
		LOGE("address space not implemented 0x%04X", address);
		gb->cpu.DEBUG_not_implemented_error = true;
		return ram;
	}
}

#pragma pack(push, 1)
struct CartridgeHeader { // start at 0x100 in file
	u8 instructions[0x4];
	u8 nintendo_graphic[0x30];
	u8 game_title[0x0F];
	u8 gameboy_color; // true iff equal to 0x80
	u8 license_h;
	u8 license_l;
	u8 super_gameboy_funcs; // supported iff equal to 0x03
	u8 cartridge_type;
	u8 cartridge_size;
	u8 ram_size;
	u8 region_code; // 0: japan, 1: non-japanese
	u8 license_code; // 33: check license_h/l, 79 Accolade, A4: Konami
	u8 mask_rom_version;
	u8 complement_check; // adjusts sum so it's 0 in the end
	u8 checksum_h;
	u8 checksum_l;

	bool isChecksumCorrect();
};
#pragma pack(pop)

bool CartridgeHeader::isChecksumCorrect() {
	u16 sum = 25;
	for (u8 *b = game_title; b != &checksum_h; b++) sum += *b;
	return (sum & 0xFF) == 0; // lower byte needs to be zero
}

void Memory::loadROM(const char *filepath) {
	init(); // reinit the memory

	rom = readDataFromFile(filepath, &rom_size);
	if (!rom) {
		rom_size = 0;
		return;
	}

	CartridgeHeader *header = (CartridgeHeader*)(rom+0x100);
	if (!header->isChecksumCorrect()) {
		LOGE("cartridge has incorrect checksum");
		delete [] rom;
		rom = nullptr;
		return;
	}

	// set MBC
	switch (header->cartridge_type) {
	case 0x00: // ROM
	case 0x08: // ROM+RAM
	case 0x09: // ROM+RAM+BATTERY
		mbc = &Memory::mbc0;
		break;
	case 0x01: // MBC1
	case 0x02: // MBC1+RAM
	case 0x03: // MBC1+RAM+BATTERY
		mbc = &Memory::mbc1;
		break;
	case 0x05: // MBC2
	case 0x06: // MBC2+BATTERY
		mbc = &Memory::mbc2;
		break;
	case 0x0B: // MMM01
	case 0x0C: // MMM01+RAM
	case 0x0D: // MMM01+RAM+BATTERY
		break;
	case 0x0F: // MBC3+TIMER+BATTERY
	case 0x10: // MBC3+TIMER+RAM+BATTERY
	case 0x11: // MBC3
	case 0x12: // MBC3+RAM
	case 0x13: // MBC3+RAM+BATTERY
		mbc = &Memory::mbc3;
		break;
	case 0x15: // MBC4
	case 0x16: // MBC4+RAM
	case 0x17: // MBC4+RAM+BATTERY
		LOGW("MBC4 not implemented");
		break;
	case 0x19: // MBC5
	case 0x1A: // MBC5+RAM
	case 0x1B: // MBC5+RAM+BATTERY
	case 0x1C: // MBC5+RUMBLE
	case 0x1D: // MBC5+RUMBLE+RAM
	case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
		LOGW("MBC5 not implemented");
		break;
	case 0xFC: // POCKET CAMERA
		LOGW("POCKET CAMERA not implemented");
		break;
	case 0xFD: // BANDAI TAMA5
		LOGW("BANDAI TAMA5 not implemented");
		break;
	case 0xFE: // HuC3
		LOGW("HuC3 not implemented");
		break;
	case 0xFF: // HuC1+RAM+BATTERY
		LOGW("HuC1 not implemented");
		break;
	default:
		LOGE("unknown cartridge type 0x%02X", header->cartridge_type);
		delete [] rom;
		rom = nullptr;
		return;
	}
	rom_bank0 = rom;
	rom_bank1 = rom+SIZE_ROM_BANK;

	// allocate SRAM
	switch (header->ram_size) {
	case 0x00: sram_size = 0x0000; break; // none
	case 0x01: sram_size = 0x0800; break; //  2 kB
	case 0x02: sram_size = 0x2000; break; //  8 kB
	case 0x03: sram_size = 0x8000; break; // 32 kB
	default: LOGW("unknown cartridge ram type", header->ram_size);
	}
	if (sram_size) {
		// check for sav file
		char sram_filepath[256];
		strcpy(sram_filepath, filepath);
		strcpy(strrchr(sram_filepath, '.'), ".sav");
		size_t sram_filesize;
		sram = readDataFromFile(sram_filepath, &sram_filesize);
		if (sram && sram_filesize != sram_size) {
			delete [] sram;
			sram = nullptr;
		}
		// no sav file
		if (!sram) {
			sram = new u8[sram_size];
			memset(sram, 0, sram_size);
		}
	}
	sram_bank = sram;
}
