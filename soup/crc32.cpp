#include "crc32.hpp"

#include "base.hpp"

#if SOUP_X86 && SOUP_BITS == 64 && defined(SOUP_USE_ASM)
#define CRC32_USE_ASM true
#else
#define CRC32_USE_ASM false
#endif

#if CRC32_USE_ASM
#include "CpuInfo.hpp"
#endif
#include "Endian.hpp"
#include "Reader.hpp"

namespace soup
{
	static const uint32_t crc32_lookup4[4][256] = {
{00, 016701630226, 035603460454, 023102250672, 0733342031, 016032572217, 035130722465, 023631112643, 01666704062, 017167134244, 034065364436, 022764554610, 01155446053, 017654276275, 034756026407, 022057616621, 03555610144, 015254020362, 036356270510, 020457440736, 03266552175, 015567362353, 036465132521, 020364702707, 02333114126, 014432724300, 037530574572, 021231344754, 02400256117, 014301466331, 037203636543, 021502006765,
07333420310, 011432210136, 032530040744, 024231670562, 07400762321, 011301152107, 032203302775, 024502532553, 06555324372, 010254514154, 033356744726, 025457174500, 06266066343, 010567656165, 033465406717, 025364236531, 04666230254, 012167400072, 031065650600, 027764060426, 04155172265, 012654742043, 031756512631, 027057322417, 05000534236, 013701304010, 030603154662, 026102764444, 05733676207, 013032046021, 030130216653, 026631426475,
016667040620, 0166670406, 023064420274, 035765210052, 016154302611, 0655532437, 023757762245, 035056152063, 017001744642, 01700174464, 022602324216, 034103514030, 017732406673, 01033236455, 022131066227, 034630656001, 015332650764, 03433060542, 020531230330, 036230400116, 015401512755, 03300322573, 020202172301, 036503742127, 014554154706, 02255764520, 021357534352, 037456304174, 014267216737, 02566426511, 021464676363, 037365046145,
011554460530, 07255250716, 024357000164, 032456630342, 011267722501, 07566112727, 024464342155, 032365572373, 010332364552, 06433554774, 025531704106, 033230134320, 010401026563, 06300616745, 025202446137, 033503276311, 012001270474, 04700440652, 027602610020, 031103020206, 012732132445, 04033702663, 027131552011, 031630362237, 013667574416, 05166344630, 026064114042, 030765724264, 013154636427, 05655006601, 026757256073, 030056466255,
035556101440, 023257731666, 0355561014, 016454351232, 035265243471, 023564473657, 0466623025, 016367013203, 034330605422, 022431035604, 01533265076, 017232455250, 034403547413, 022302377635, 01200127047, 017501717261, 036003711504, 020702121722, 03600371150, 015101541376, 036730453535, 020031263713, 03133033161, 015632603347, 037665015566, 021164625740, 02066475132, 014767245314, 037156357557, 021657567771, 02755737103, 014054107325,
032665521750, 024164311576, 07066141304, 011767771122, 032156663761, 024657053547, 07755203335, 011054433113, 033003225732, 025702415514, 06600645366, 010101075140, 033730167703, 025031757525, 06133507357, 010632337171, 031330331614, 027431501432, 04533751240, 012232161066, 031403073625, 027302643403, 04200413271, 012501223057, 030556435676, 026257205450, 05355055222, 013454665004, 030265777647, 026564147461, 05466317213, 013367527035,
023331141260, 035430771046, 016532521634, 0233311412, 023402203251, 035303433077, 016201663605, 0500053423, 022557645202, 034256075024, 017354225656, 01455415470, 022264507233, 034565337015, 017467167667, 01366757441, 020664751324, 036165161102, 015067331770, 03766501556, 020157413315, 036656223133, 015754073741, 03055643567, 021002055346, 037703665160, 014601435712, 02100205534, 021731317377, 037030527151, 014132777723, 02633147505,
024002561170, 032703351356, 011601101524, 07100731702, 024731623141, 032030013367, 011132243515, 07633473733, 025664265112, 033165455334, 010067605546, 06766035760, 025157127123, 033656717305, 010754547577, 06055377751, 027557371034, 031256541212, 012354711460, 04455121646, 027264033005, 031565603223, 012467453451, 04366263677, 026331475056, 030430245270, 013532015402, 05233625624, 026402737067, 030303107241, 013201357433, 05500567615,
}, { 00,03106630501,06215461202,05313251703,014433142404,017535772105,012626523606,011720313307,031066305010,032160535511,037273764212,034375154713,025455247414,026553477115,023640626616,020746016317,011260411121,012366221420,017075070323,014173640622,05653553525,06755363024,03446132727,0540702226,020206714131,023300124430,026013375333,025115545632,034635656535,037733066034,032420237737,031526407236,
022541022242,021447612743,024754443040,027652273541,036172160646,035074750347,030367501444,033261331145,013527327252,010421517753,015732746050,016634176551,07114265656,04012455357,01301604454,02207034155,033721433363,030627203662,035534052161,036432662460,027312571767,024214341266,021107110565,022001720064,02747736373,01641106672,04552357171,07454567470,016374674777,015272044276,010161215575,013067425074,
036036247405,035130477104,030223626607,033325016306,022405305001,021503535500,024610764203,027716154702,07050142415,04156772114,01245523617,02343313316,013463000011,010565630510,015676461213,016770251712,027256656524,024350066025,021043237726,022145407227,033665714120,030763124421,035470375322,036576545623,016230553534,015336363035,010025132736,013123702237,02603411130,01705221431,04416070332,07510640633,
014577265647,017471455346,012762604445,011664034144,0144327243,03042517742,06351746041,05257176540,025511160657,026417750356,023704501455,020602331154,031122022253,032024612752,037337443051,034231273550,05717674766,06611044267,03502215564,0404425065,011324736362,012222106663,017131357160,014037567461,034771571776,037677341277,032564110574,031462720075,020342433372,023244203673,026157052170,025051662471,
07340714113,04246124412,01155375311,02053545610,013773656517,010675066016,015566237715,016460407214,036326411103,035220221402,030133070301,033035640600,022715553507,021613363006,024500132705,027406702204,016120305032,015026535533,010335764230,013233154731,02513247436,01415477137,04706626634,07600016335,027146000022,024040630523,021353461220,022255251721,033575142426,030473772127,035760523624,036666313325,
025601736351,026707106650,023414357153,020512567452,031232674755,032334044254,037027215557,034121425056,014667433341,017761203640,012472052143,011574662442,0254571745,03352341244,06041110547,05147720046,034461327270,037567517771,032674746072,031772176573,020052265674,023154455375,026247604476,025341034177,05407022260,06501612761,03612443062,0714273563,011034160664,012132750365,017221501466,014327331167,
031376553516,032270363017,037163132714,034065702215,025745411112,026643221413,023550070310,020456640611,0310656506,03216066007,06105237704,05003407205,014723714102,017625124403,012536375300,011430545601,020116142437,023010772136,026303523635,025205313334,034525000033,037423630532,032730461231,031636251730,011170247427,012076477126,017365626625,014263016324,05543305023,06445535522,03756764221,0650154720,
013637571754,010731341255,015422110556,016524720057,07204433350,04302203651,01011052152,02117662453,022651674744,021757044245,024444215546,027542425047,036262736340,035364106641,030077357142,033171567443,02457160675,01551750374,04642501477,07744331176,016064022271,015162612770,010271443073,013377273572,033431265665,030537455364,035624604467,036722034166,027002327261,024104517760,021217746063,022311176562,
}, { 00,0160465067,0341152156,0221537131,0702324334,0662741353,0443276262,0523613205,01604650670,01764235617,01545702726,01425367741,01106574544,01066111523,01247426412,01327043475,03411521560,03571144507,03750473436,03630016451,03313605654,03273260633,03052757702,03132332765,02215371310,02375714377,02154223246,02034646221,02517055024,02477430043,02656107172,02736562115,
07023243340,07143626327,07362311216,07202774271,07721167074,07641502013,07460035122,07500450145,06627413530,06747076557,06566541466,06406124401,06125737604,06045352663,06264665752,06304200735,04432762620,04552307647,04773630776,04613255711,04330446514,04250023573,04071514442,04111171425,05236132050,05356557037,05177060106,05017405161,05534216364,05454673303,05675344232,05715721255,
016046506700,016126163767,016307454656,016267031631,016744622434,016624247453,016405770562,016565315505,017642356170,017722733117,017503204026,017463661041,017140072244,017020417223,017201120312,017361545375,015457027260,015537442207,015716175336,015676510351,015355303154,015235766133,015014251002,015174634065,014253677410,014333212477,014112725546,014072340521,014551553724,014431136743,014610401672,014770064615,
011065745440,011105320427,011324617516,011244272571,011767461774,011607004713,011426533622,011546156645,010661115230,010701570257,010520047366,010440422301,010163231104,010003654163,010222363052,010342706035,012474264120,012514601147,012735336076,012655753011,012376140214,012216525273,012037012342,012157477325,013270434750,013310051737,013131566606,013051103661,013572710464,013412375403,013633642532,013753227555,
034115215600,034075670667,034254347756,034334722731,034617131534,034777554553,034556063462,034436406405,035711445070,035671020017,035450517126,035530172141,035013761344,035173304323,035352633212,035232256275,037504734360,037464351307,037645666236,037725203251,037206410054,037366075033,037147542102,037027127165,036300164510,036260501577,036041036446,036121453421,036402240624,036562625643,036743312772,036623777715,
033136056540,033056433527,033277104416,033317561471,033634372674,033754717613,033575220722,033415645745,032732606330,032652263357,032473754266,032513331201,032030522004,032150147063,032371470152,032211015135,030527577020,030447112047,030666425176,030706040111,030225653314,030345236373,030164701242,030004364225,031323327650,031243742637,031062275706,031102610761,031421003564,031541466503,031760151432,031600534455,
022153713100,022033376167,022212641056,022372224031,022651437234,022731052253,022510565362,022470100305,023757143770,023637526717,023416011626,023576474641,023055267444,023135602423,023314335512,023274750575,021542232460,021422657407,021603360536,021763705551,021240116754,021320573733,021101044602,021061421665,020346462210,020226007277,020007530346,020167155321,020444746124,020524323143,020705614072,020665271015,
025170550240,025010135227,025231402316,025351067371,025672674174,025712211113,025533726022,025453343045,024774300430,024614765457,024435252566,024555637501,024076024704,024116441763,024337176652,024257513635,026561071720,026401414747,026620123676,026740546611,026263355414,026303730473,026122207542,026042662525,027365621150,027205244137,027024773006,027144316061,027467505264,027507160203,027726457332,027646032355,
}, { 00,027057063545,025202344213,02255327756,021730513527,06767570062,04532657734,023565634271,030555024357,017502047612,015757360144,032700303401,011265537670,036232554335,034067673463,013030610126,012006253637,035051230372,037204117424,010253174161,033736740310,014761723655,016534404103,031563467446,022553277560,05504214025,07751133773,020706150236,03263764047,024234707502,026061420254,01036443711,
024014527476,03043544133,01216663665,026241600320,05724034151,022773057414,020526370342,07571313607,014541503721,033516560264,031743647532,016714624077,035271010206,012226073743,010073354015,037024337550,036012774241,011045717704,013210430052,034247453517,017722267766,030775204223,032520123575,015577140030,06547750116,021510733453,023745414305,04712477640,027277243431,0220220174,02075107622,025022164367,
023305054075,04352037530,06107310266,021150373723,02435547552,025462524017,027637603741,0660660204,013650070322,034607013667,036452334131,011405357474,032160563605,015137500340,017362627416,030335644153,031303207642,016354264307,014101143451,033156120114,010433714365,037464777620,035631450176,012666433433,01656223515,026601240050,024454167706,03403104243,020166730032,07131753577,05364474221,022333417764,
07311573403,020346510146,022113637610,05144654355,026421060124,01476003461,03623324337,024674347672,037644557754,010613534211,012446613547,035411670002,016174044273,031123027736,033376300060,014321363525,015317720234,032340743771,030115464027,017142407562,034427233713,013470250256,011625177500,036672114045,025642704163,02615767426,0440440370,027417423635,04172217444,023125274101,021370153657,06327130312,
035526333073,012571350536,010724077260,037773014725,014216620554,033241643011,031014564747,016043507202,05073317324,022024374661,020271053137,07226030472,024743604603,03714667346,01541540410,026516523155,027520160644,0577103301,02722224457,025775247112,06210473363,021247410626,023012737170,04045754435,017075144513,030022127056,032277200700,015220263245,036745457034,011712434571,013547713227,034510770762,
011532614405,036565677140,034730550616,013767533353,030202307122,017255364467,015000043331,032057020674,021067630752,06030653217,04265574541,023232517004,0757323275,027700340730,025555067066,02502004523,03534447232,024563424777,026736703021,01761760564,022204154715,05253137250,07006210506,020051273043,033061463165,014036400420,016263727376,031234744633,012751170442,035706113107,037553234651,010504257314,
016623367006,031674304543,033421023215,014476040750,037113674521,010144617064,012311530732,035346553277,026376343351,01321320614,03174007142,024123064407,07446650676,020411633333,022644514465,05613577120,04625134631,023672157374,021427270422,06470213167,025115427316,02142444653,0317763105,027340700440,034370110566,013327173023,011172254775,036125237230,015440403041,032417460504,030642747252,017615724717,
032637640470,015660623135,017435504663,030462567326,013107353157,034150330412,036305017344,011352074601,02362664727,025335607262,027160520534,0137543071,023452377200,04405314745,06650033013,021607050556,020631413247,07666470702,05433757054,022464734511,01101100760,026156163225,024303244573,03354227036,010364437110,037333454455,035166773303,012131710646,031454124437,016403147172,014656260624,033601203361,
} };

	uint32_t crc32::hash(Reader& r)
	{
		uint32_t checksum = 0xFFFFFFFFul;
		for (uint8_t c; r.u8(c); )
		{
			checksum = (checksum >> 8) ^ crc32_lookup4[0][(checksum & 0xFF) ^ c];
		}
		checksum ^= 0xFFFFFFFFu;
		return checksum;
	}

	uint32_t crc32::hash(const std::string& data)
	{
		return hash((const uint8_t*)data.data(), data.size(), INITIAL);
	}

	static uint32_t crc32_slice_by_4(const uint8_t* data, size_t size, uint32_t init)
	{
		uint32_t checksum = ~init;
		const uint32_t* data32 = reinterpret_cast<const uint32_t*>(data);

		for (; size >= 4; ++data32, size -= 4)
		{
			uint32_t v = checksum;
			if constexpr (NATIVE_ENDIAN == LITTLE_ENDIAN)
			{
				v ^= *data32;
			}
			else
			{
				v ^= Endianness::invert(*data32);
			}
			checksum = crc32_lookup4[0][v >> 24] ^ crc32_lookup4[1][(v >> 16) & 0xFF] ^ crc32_lookup4[2][(v >> 8) & 0xFF] ^ crc32_lookup4[3][v & 0xFF];
		}

		for (const uint8_t* data8 = reinterpret_cast<const uint8_t*>(data32); size; --size)
			checksum = (checksum >> 8) ^ crc32_lookup4[0][(checksum & 0xFF) ^ *data8++];

		return ~checksum;
	}

	// Unfortunately, to use these intrinsics, we need to set the appropriate compiler flags.
	// However, setting these compiler flags tells the optimiser that it can go ahead and use these instructions without first checking
	// if the CPU supports them, like we do here, causing low-end users to be unable to use our software at all.
	// So, we have .asm files to use these instructions with intent. If you have an assembler and want to link against the them, define SOUP_USE_ASM.
	// With the Visual Studio solution, this "just works."

#if CRC32_USE_ASM
	extern "C" __fastcall uint32_t crc32_pclmul(const uint8_t* p, size_t size, uint32_t crc);

	/*static __declspec(noinline) uint32_t crc32_pclmul(const uint8_t* p, size_t size, uint32_t crc)
	{
#ifdef _MSC_VER
		static const uint64_t __declspec(align(16))
#else
		static const uint64_t __attribute__((aligned(16)))
#endif
			s_u[2] = { 0x1DB710641, 0x1F7011641 }, s_k5k0[2] = { 0x163CD6124, 0 }, s_k3k4[2] = { 0x1751997D0, 0xCCAA009E };

		// Load first 16 bytes, apply initial CRC32
		__m128i b = _mm_xor_si128(_mm_cvtsi32_si128(~crc), _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));

		// We're skipping directly to Step 2 page 12 - iteratively folding by 1 (by 4 is overkill for our needs)
		const __m128i k3k4 = _mm_load_si128(reinterpret_cast<const __m128i*>(s_k3k4));

		for (size -= 16, p += 16; size >= 16; size -= 16, p += 16)
			b = _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(b, k3k4, 17), _mm_loadu_si128(reinterpret_cast<const __m128i*>(p))), _mm_clmulepi64_si128(b, k3k4, 0));

		// Final stages: fold to 64-bits, 32-bit Barrett reduction
		const __m128i z = _mm_set_epi32(0, ~0, 0, ~0), u = _mm_load_si128(reinterpret_cast<const __m128i*>(s_u));
		b = _mm_xor_si128(_mm_srli_si128(b, 8), _mm_clmulepi64_si128(b, k3k4, 16));
		b = _mm_xor_si128(_mm_clmulepi64_si128(_mm_and_si128(b, z), _mm_loadl_epi64(reinterpret_cast<const __m128i*>(s_k5k0)), 0), _mm_srli_si128(b, 4));
		return ~_mm_extract_epi32(_mm_xor_si128(b, _mm_clmulepi64_si128(_mm_and_si128(_mm_clmulepi64_si128(_mm_and_si128(b, z), u, 16), z), u, 0)), 1);
	}*/

	static uint32_t crc32_sse41_simd(const uint8_t* data, size_t size, uint32_t init)
	{
		if (size < 16)
		{
			return crc32_slice_by_4(data, size, init);
		}

		uint32_t simd_len = size & ~15;
		uint32_t c = crc32_pclmul(data, simd_len, init);
		return crc32_slice_by_4(data + simd_len, size - simd_len, c);
	}
#endif

	uint32_t crc32::hash(const uint8_t* data, size_t size, uint32_t init)
	{
#if CRC32_USE_ASM
		const CpuInfo& cpu_info = CpuInfo::get();
		if (cpu_info.supportsPCLMULQDQ()
			&& cpu_info.supportsSSE4_1()
			)
		{
			return crc32_sse41_simd(data, size, init);
		}
#endif
		return crc32_slice_by_4(data, size, init);
	}
}
