#include "psuedo.h"
#include "eval.h"

#define CLASS TPsuedoOp

CLASS::CLASS()
{

}

CLASS::~CLASS()
{

}

int CLASS::doLUP(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);
	UNUSED(line);
	UNUSED(a);
	int lidx, len;
	int res = 0;
	int err = 0;

	std::string op = Poco::toUpper(line.opcode);

	if (op == "LUP")
	{
		len = line.lineno - 1; // MerlinLine line numbers are +1 from actual array idx
		if (len >= 0)
		{
			a.LUPstack.push(a.curLUP);

			a.curLUP.lupoffset = len;
			a.curLUP.lupct = 3; // evaluate here
			a.curLUP.luprunning++;
		}
		else
		{
			printf("err 3\n");
			err = errUnexpectedOp;
		}
	}

	if (op == "--^")
	{
		if (a.curLUP.luprunning > 0)
		{
			lidx = line.lineno - 1;
			len = lidx - a.curLUP.lupoffset - 1;

			if (a.curLUP.lupct > 0)
			{
				a.curLUP.lupct--;
				if (a.curLUP.lupct != 0)
				{
					a.lineno = a.curLUP.lupoffset;
					goto out;
				}
			}

			//printf("start=%d end=%d len=%d\n", a.curLUP.lupoffset, lidx, len);
			if (a.LUPstack.size() > 0)
			{
				a.curLUP = a.LUPstack.top();
				a.LUPstack.pop();
			}
			else
			{
				printf("err 2\n");
				err = errUnexpectedOp;
			}
		}
		else
		{
			printf("err 1\n");
			err = errUnexpectedOp;
		}
	}

out:
	if (err > 0)
	{
		line.setError(err);
	}
	return (res);
}

constexpr unsigned int strhash(const char *str, int h = 0)
{
	return !str[h] ? 5381 : (strhash(str, h + 1) * 33) ^ str[h];
}

int CLASS::doDATA(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);
	TEvaluator eval(a);

	int i;
	int outct = 0;
	int wordsize = 2;
	int endian = 0;
	std::string oper = line.operand;
	std::string op = Poco::toUpper(Poco::trim(line.opcode));
	Poco::StringTokenizer tok(oper, ",", Poco::StringTokenizer::TOK_TRIM |
	                          Poco::StringTokenizer::TOK_IGNORE_EMPTY);


	const char *ptr = (const char *)op.c_str();
	switch (strhash(ptr) )
	{
		case strhash((const char *)"DA"):
		case strhash((const char *)"DW"):
			wordsize = 2;
			break;
		case strhash((const char *)"DDB"):
			wordsize = 2;
			endian = 1;
			break;
		case strhash((const char *)"DFB"):
		case strhash((const char *)"DB"):
			wordsize = 1;
			break;
		case strhash((const char *)"ADR"):
			wordsize = 3;
			break;
		case strhash((const char *)"ADRL"):
			wordsize = 4;
			break;
		default:
			wordsize = 0;
			break;
	}

	for (auto itr = tok.begin(); itr != tok.end(); ++itr)
	{
		//printf("%s\n",(*itr).c_str());
		//evaluate each of these strings, check for errors on pass 2

		std::string expr = *itr;
		int64_t eval_result = 0;
		uint8_t shift;
		int r;
		uint8_t b;

		shift = 0;
		r = eval.evaluate(expr, eval_result, shift);
		if (r < 0)
		{
			//printf("eval error %d |%s|\n", r,expr.c_str());
			if (a.pass > 0)
			{
				line.setError(errBadEvaluation);
			}
		}
		if (shift == '>')
		{
			eval_result = (eval_result) & 0xFF;
		}
		if (shift == '<')
		{
			eval_result = (eval_result >> 8) & 0xFF;
		}
		else if ((shift == '^') || (shift == '|'))
		{
			eval_result = (eval_result >> 16) & 0xFF;
		}


		outct += wordsize;
		if (a.pass > 0)
		{
			if (!endian) // little endian
			{
				for (i = 0; i < wordsize; i++)
				{
					b = (eval_result >> (8 * i)) & 0xFF;
					line.outbytes.push_back(b);
					//printf("%02X\n",b);
				}
			}
			else
			{
				// big endian
				for (i = 0; i < wordsize; i++)
				{
					b = (eval_result >> ((wordsize - 1 - i) * 8)) & 0xFF;
					line.outbytes.push_back(b);
					//printf("%02X\n",b);
				}

			}
		}
	}
	line.outbytect = outct;
	return (outct);
}

int CLASS::doDS(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);

	int res = 0;
	int32_t v = line.expr_value;
	if (line.eval_result != 0)
	{
		line.setError(errForwardRef);
	}
	else if ((v < 0) || ((a.PC.currentpc + v) >= 0x10000)) // no neg, or crossing bank bound
	{
		line.setError(errOverflow);
	}
	else
	{
		res = v;

		line.datafillbyte = line.eval_result & 0xFF;
		line.datafillct = v;
#if 0
		if (a.pass > 0)
		{
			for (int i = 0; i < v; i++)
			{
				line.outbytes.push_back(0x00);
			}
			line.outbytect = v;
		}
#endif

	}
	return (res);
}

int CLASS::doDUM(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);

	int res = 0;
	bool isdend = ((opinfo.opcode == P_DEND) ? true : false);

	if (!isdend)
	{
		a.dumstart = 1;
		a.dumstartaddr = line.expr_value;
	}
	else
	{
		a.dumstart = -1;
		if (a.PCstack.size() == 0)
		{
			line.setError(errBadDUMop);
			a.dumstart = 0;
		}
	}

	return (res);
}

int CLASS::doLST(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);

	std::string s;
	if (a.pass > 0)
	{
		s = Poco::toUpper(Poco::trim(line.operand));
		if ((s == "") || (s == "ON") || (line.expr_value > 0))
		{
			//printf("ON\n");
			a.skiplist = true;
			a.listing = true;
		}
		else if ((s == "OFF") || (line.expr_value == 0))
		{
			//printf("OFF\n");
			a.skiplist = true;
			a.listing = false;
		}
	}
	return (0);
}

int CLASS::doHEX(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	UNUSED(opinfo);

	std::string os = Poco::toUpper(Poco::trim(line.operand));

	uint32_t bytect = 0;
	uint8_t b = 0;
	uint8_t ct = 0;
	for ( uint32_t i = 0; i < os.length(); ++i )
	{
		char c = os[i];

		if ((c >= '0') && (c <= '9'))
		{
			c = c - '0';
		}
		else if ((c >= 'a') && (c <= 'f'))
		{
			c = c - 'a' + 10;
		}
		else if ((c >= 'A') && (c <= 'F'))
		{
			c = c - 'A' + 10;
		}
		else if (c == ',')
		{
			continue;
		}
		else
		{
			line.setError(errBadCharacter);
			return 0;
		}

		// Got a good char, append to hex string and see if we've got a byte
		switch (ct)
		{
			case 0:
				b = (c << 4);
				break;
			case 1:
				b |= c;
				break;
		}
		ct = (ct + 1) & 0x01;
		if (!ct)
		{
			if (a.pass > 0)
			{
				line.outbytes.push_back(b);
			}
			b = 0;
			bytect++;
		}
	}
	if (ct & 0x01) // we got an odd number of nibbles
	{
		line.setError(errMalformed);
		bytect = 0;
	}
	line.outbytect = bytect;
	return bytect;
}


int CLASS::ProcessOpcode(T65816Asm &a, MerlinLine &line, TSymbol &opinfo)
{
	int res = 0;

	switch (opinfo.opcode)
	{
		default:
			res = -1; // undefined p-op
			line.setError(errUnimplemented);
			break;
		case P_DS:
			res = doDS(a, line, opinfo);
			break;
		case P_PUT:
		case P_USE:
			// both of these are handled by the input file processor, just allow them to be
			// processed with no errors here
			break;
		case P_DUM:
		case P_DEND:
			res = doDUM(a, line, opinfo);
			line.flags|=FLAG_FORCEADDRPRINT;

			break;
		case P_ORG:
			if (line.operand.length() > 0)
			{
				a.PC.orgsave = a.PC.currentpc;
				a.PC.currentpc = line.expr_value;
				line.startpc = line.expr_value;
			}
			else
			{
				a.PC.currentpc = a.PC.orgsave;
				line.startpc = a.PC.orgsave;
			}
			line.flags|=FLAG_FORCEADDRPRINT;
			break;
		case P_SAV:
			a.savepath = a.processFilename(line.operand, Poco::Path::current(), 0);
			break;
		case P_LST:
			res = doLST(a, line, opinfo);
			break;
		case P_HEX:
			res = doHEX(a, line, opinfo);
			break;
		case P_DATA:
			res = doDATA(a, line, opinfo);
			break;
		case P_LUP:
			res = doLUP(a, line, opinfo);
			res = 0;
			break;
	}
	return (res);
}
