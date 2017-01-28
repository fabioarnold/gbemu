// addresses
const int ADR_ROM_BANK0           = 0x0000;
const int ADR_ROM_BANK1           = 0x4000;
const int ADR_VRAM                = 0x8000;
const int ADR_RAM_EXTERNAL        = 0xA000;
const int ADR_RAM_INTERNAL_BANK0  = 0xC000;
const int ADR_RAM_INTERNAL_BANK1  = 0xD000; // CGB: switchable (1-7)
const int ADR_RAM_INTERNAL_MIRROR = 0xE000;
const int ADR_OAM                 = 0xFE00;
const int ADR_EMPTY               = 0xFEA0;
const int ADR_UNUSABLE            = 0xFEA0;
const int ADR_IO                  = 0xFF00;
const int ADR_HRAM                = 0xFF80;
const int ADR_IE                  = 0xFFFF;

// sizes
const int SIZE_BOOT_ROM = 0x0100;  // DMG
const int SIZE_ROM_BANK = 0x4000;
const int SIZE_VRAM     = 0x2000;
const int SIZE_RAM      = 0x2000;
const int SIZE_OAM      = 0x00A0;
const int SIZE_IO       = 0x0080;
const int SIZE_HRAM     = 0x007F;

#include "oam.h"
#include "io.h"

struct Memory {
	u8 boot_rom[SIZE_BOOT_ROM]; // 0x0000
	u8 *rom = nullptr; // 32kB, 64kB, 128kB, 256kB, 512kB and so on
	size_t rom_size;
	u8 *rom_bank0 = nullptr;
	u8 *rom_bank1 = nullptr;
	u8 *sram = nullptr; // 2kB, 8kB, 32kB
	size_t sram_size;
	u8 *sram_bank;
	u8 vram[SIZE_VRAM]; // 0x8000
	u8  ram[SIZE_RAM];  // 0xC000
	OAM oam;            // 0xFE00
	IO io;              // 0xFF00
	u8 hram[SIZE_HRAM]; // 0xFF80

	bool sram_enabled = false;

	void init();
	void reset(); // doesn't clear ROM
	void loadROM(const char *filepath);

	u8 load8(u16 address);
	void store8(u16 address, u8 value);

	void DMAtoOAM(u16 src_address); // transfer 160 bytes from src_address to oam

	// memory bank controller
	typedef void (Memory::*MBC)(u16 address, u8 value);
	MBC mbc; // set on loadROM
	void mbc0(u16 address, u8 value); // dummy MBC
	void mbc1(u16 address, u8 value);
	void mbc3(u16 address, u8 value);
	void mbc5(u16 address, u8 value);

	void setROMBank(u8 bank);
	void setSRAMBank(u8 bank);

	GameBoy *gb;
private:
	u8 *map(u16 address);
};
