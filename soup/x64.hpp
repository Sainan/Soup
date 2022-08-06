#pragma once

#include <cstdint>
#include <string>

namespace soup
{
	struct x64
	{
		enum Register : uint8_t
		{
			RA = 0,
			RC,
			RB,
			RD,
			SP,
			BP,
			SI,
			DI,
			R8,
			R9,
			R10,
			R11,
			R12,
			R13,
			R14,
			R15,
		};

		enum RegisterAccessType : uint8_t
		{
			ACCESS_64 = 64,
			ACCESS_32 = 32,
			ACCESS_16 = 16,
			ACCESS_8 = 8,
			ACCESS_8_H = 0,
		};

		struct Operand
		{
			Register reg;
			RegisterAccessType access_type;
			uint8_t deref_size = 0;

			void decode(bool rex, uint8_t size, uint8_t reg, bool x) noexcept;

			[[nodiscard]] std::string toString() const;
		};

		enum OperandEncoding : uint8_t
		{
			ZO = 0,

			O = 0b01,
			M = 0b10,
			R = 0b11,

			OPERAND_MASK = 0b11,
			BITS_PER_OPERAND = 2,

			MR = M | (R << BITS_PER_OPERAND),
			RM = R | (M << BITS_PER_OPERAND),
		};

		struct Operation
		{
			const char* const name;
			const uint8_t opcode;
			const OperandEncoding operand_encoding;
			const uint8_t operand_size;

			Operation(const char* name, uint8_t opcode, OperandEncoding operand_encoding)
				: name(name), opcode(opcode), operand_encoding(operand_encoding), operand_size(0)
			{
			}
			
			Operation(const char* name, uint8_t opcode, OperandEncoding operand_encoding, uint8_t operand_size)
				: name(name), opcode(opcode), operand_encoding(operand_encoding), operand_size(operand_size)
			{
			}

			[[nodiscard]] bool matches(uint8_t code) const noexcept
			{
				if (getOpr1Encoding() == O)
				{
					code &= 0b11111000;
				}
				return opcode == code;
			}

			[[nodiscard]] OperandEncoding getOpr1Encoding() const noexcept
			{
				return (OperandEncoding)(operand_encoding & OPERAND_MASK);
			}

			[[nodiscard]] OperandEncoding getOpr2Encoding() const noexcept
			{
				return (OperandEncoding)((operand_encoding >> BITS_PER_OPERAND) & OPERAND_MASK);
			}

			[[nodiscard]] uint8_t getNumOperands() const noexcept
			{
				if (getOpr1Encoding() == ZO)
				{
					return 0;
				}
				if (getOpr2Encoding() == ZO)
				{
					return 1;
				}
				return 2;
			}
		};

		inline static Operation operations[] = {
			{ "mov", 0x88, MR, 8 },
			{ "mov", 0x89, MR },
			{ "mov", 0x8A, RM, 8 },
			{ "mov", 0x8B, RM },
			{ "ret", 0xC3, ZO },
			{ "push", 0x50, O, 64 },
			{ "push", 0xFF, M, 64 },
		};

		struct Instruction
		{
			const Operation* operation;
			Operand operands[2];

			[[nodiscard]] uint8_t getNumOperands() const noexcept
			{
				return operation->getNumOperands();
			}

			[[nodiscard]] std::string toString() const;
		};

		[[nodiscard]] static Instruction disasm(const uint8_t*& code);
	};
}
