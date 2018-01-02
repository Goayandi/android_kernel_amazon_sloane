#ifndef MT_SPOWER_CPU_H
#define MT_SPOWER_CPU_H



#define VSIZE 9
#define TSIZE 20
#define MAX_TABLE_SIZE 3

/**  PLEASE MAKE SURE the following things for table interpolation:
 * 1. table order: FF, TT, SS; such that, the order is necessarily obey as following:
 *       the power at (1150, 30) shoule be descent absoultely.
 **/

#define CA15L_TABLE_0                                                     \
	/* "(WAT 11.57%) Leakage Power"	 */                             \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	121,	162,	214,	283,	373,	490,	643,	730,	829, \
		30,	140,	186,	248,	330,	434,	562,	728,	833,	948, \
		35,	161,	215,	287,	380,	495,	647,	840,	955,	1083, \
		40,	185,	249,	331,	434,	568,	742,	952,	1082,	1229, \
		45,	213,	285,	378,	497,	648,	841,	1089,	1240,	1410, \
		50,	248,	329,	434,	568,	740,	963,	1249,	1420,	1611, \
		55,	286,	377,	496,	650,	850,	1104,	1429,	1623,	1844, \
		60,	327,	432,	569,	747,	972,	1263,	1641,	1867,	2125, \
		65,	375,	497,	654,	854,	1117,	1457,	1890,	2157,	2455, \
		70,	432,	570,	750,	985,	1288,	1677,	2183,	2482,	2819, \
		75,	498,	660,	868,	1136,	1484,	1932,	2503,	2842,	3224, \
		80,	578,	762,	1004,	1314,	1711,	2223,	2863,	3249,	3685, \
		85,	667,	881,	1157,	1510,	1970,	2555,	3292,	3736,	4239, \
		90,	766,	1012,	1328,	1738,	2264,	2938,	3789,	4308,	4888, \
		95,	880,	1161,	1525,	1997,	2599,	3368,	4339,	4926,	5588, \
		100,	1015,	1338,	1753,	2288,	2972,	3859,	4977,	5652,	6417, \
		105,	1176,	1544,	2021,	2633,	3420,	4410,	5687,	6448,	7312, \
		110,	1355,	1785,	2330,	3031,	3920,	5066,	6504,	7364,	8361, \
		115,	1559,	2056,	2688,	3496,	4511,	5820,	7469,	8449,	9562, \
		120,	1800,	2368,	3093,	4019,	5197,	6699,	8596,	9722,	10985

#define CA15L_TABLE_1                                                   \
	/* "(WAT 0.78%) Leakage Power"  */                              \
	/**/	        800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	30,	39,	50,	66,	85,	109,	134,	147,	161, \
		30,	33,	44,	58,	75,	98,	121,	148,	164,	182, \
		35,	38,	51,	66,	85,	109,	135,	166,	185,	203, \
		40,	44,	58,	75,	96,	121,	151,	188,	207,	228, \
		45,	50,	66,	85,	109,	137,	172,	210,	232,	255, \
		50,	58,	75,	97,	124,	156,	192,	235,	260,	288, \
		55,	66,	86,	111,	141,	175,	218,	269,	297,	327, \
		60,	76,	100,	126,	159,	201,	250,	307,	339,	373, \
		65,	88,	113,	143,	184,	229,	285,	350,	385,	423, \
		70,	100,	129,	166,	210,	263,	325,	397,	438,	481, \
		75,	115,	149,	190,	241,	302,	374,	456,	499,	550, \
		80,	133,	172,	219,	277,	348,	429,	524,	577,	628, \
		85,	154,	198,	253,	320,	401,	496,	605,	666,	731, \
		90,	177,	229,	294,	370,	462,	571,	699,	769,	845, \
		95,	205,	268,	339,	427,	533,	661,	806,	887,	974, \
		100,	242,	310,	393,	495,	618,	764,	931,	1024,	1124, \
		105,	282,	362,	459,	576,	721,	887,	1086,	1192,	1315, \
		110,	331,	426,	540,	677,	842,	1045,	1277,	1411,	1529, \
		115,	387,	499,	631,	800,	997,	1237,	1505,	1640,	1804, \
		120,	450,	584,	751,	945,	1183,	1458,	1767,	1945,	2136


#define CA15L_TABLE_2							\
	/*"(WAT -8.16%) Leakage Power" */                               \
	/**/	        800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	24,	29,	35,	44,	55,	70,	88,	100,	113, \
		30,	26,	32,	39,	48,	60,	76,	98,	110,	124, \
		35,	29,	34,	43,	53,	67,	85,	106,	119,	134, \
		40,	32,	39,	48,	60,	74,	93,	116,	131,	147, \
		45,	36,	44,	54,	66,	82,	103,	128,	143,	161, \
		50,	40,	49,	60,	74,	92,	113,	141,	158,	178, \
		55,	45,	55,	67,	82,	101,	125,	155,	175,	195, \
		60,	52,	62,	75,	92,	113,	139,	174,	194,	217, \
		65,	58,	70,	84,	102,	125,	155,	192,	216,	243, \
		70,	65,	78,	94,	115,	141,	174,	217,	242,	271, \
		75,	74,	89,	108,	131,	160,	196,	243,	271,	303, \
		80,	85,	101,	122,	148,	181,	223,	277,	309,	345, \
		85,	96,	114,	138,	167,	204,	252,	313,	349,	390, \
		90,	110,	131,	159,	192,	234,	288,	358,	401,	449, \
		95,	127,	151,	181,	219,	267,	329,	410,	459,	516, \
		100,	147,	174,	208,	253,	313,	386,	480,	539,	606, \
		105,	169,	204,	244,	298,	365,	453,	561,	625,	700, \
		110,	198,	236,	286,	347,	426,	527,	658,	738,	828, \
		115,	233,	279,	338,	410,	502,	621,	776,	873,	982, \
		120,	270,	327,	397,	484,	592,	736,	912,	1019,	1145



#define CA7_TABLE_0							\
	/* "(WAT 11.57%) Leakage Power"	 */                             \
	/**/	        800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	35,	45,	58,	75,	97,	125,	162,	185,	211, \
		30,	40,	51,	66,	86,	110,	141,	184,	210,	239, \
		35,	45,	58,	76,	98,	126,	162,	210,	238,	271, \
		40,	51,	66,	86,	110,	142,	184,	238,	271,	308, \
		45,	59,	76,	97,	125,	163,	209,	270,	307,	349, \
		50,	67,	86,	111,	144,	185,	237,	305,	347,	393, \
		55,	77,	99,	127,	164,	210,	269,	346,	392,	445, \
		60,	88,	114,	145,	186,	238,	306,	392,	444,	505, \
		65,	101,	129,	166,	212,	271,	347,	445,	505,	575, \
		70,	115,	148,	189,	241,	309,	395,	507,	575,	654, \
		75,	133,	169,	216,	275,	351,	449,	578,	656,	745, \
		80,	151,	193,	245,	313,	401,	515,	661,	748,	847, \
		85,	174,	222,	282,	360,	460,	588,	754,	853,	967, \
		90,	199,	253,	323,	413,	528,	675,	864,	973,	1101, \
		95,	227,	290,	370,	472,	601,	769,	983,	1112,	1257, \
		100,	261,	331,	421,	538,	690,	879,	1122,	1270,	1437, \
		105,	299,	381,	483,	615,	778,	996,	1280,	1445,	1633, \
		110,	344,	437,	554,	700,	889,	1136,	1455,	1650,	1866, \
		115,	395,	502,	636,	800,	1019,	1300,	1661,	1870,	2125, \
		120,	450,	574,	729,	918,	1169,	1491,	1894,	2134,	2411



#define CA7_TABLE_1							\
	/* "(WAT 0.78%) Leakage Power" */                               \
	/**/	        800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	11,	13,	16,	20,	25,	31,	38,	42,	48, \
		30,	12,	15,	18,	22,	27,	34,	42,	47,	53, \
		35,	13,	16,	20,	24,	30,	38,	47,	53,	58, \
		40,	15,	18,	22,	27,	34,	42,	52,	58,	65, \
		45,	17,	20,	25,	31,	38,	47,	58,	65,	72, \
		50,	19,	23,	29,	35,	43,	53,	65,	72,	81, \
		55,	21,	26,	32,	40,	48,	59,	73,	81,	91, \
		60,	25,	30,	36,	44,	54,	66,	82,	91,	102, \
		65,	28,	34,	41,	49,	61,	75,	93,	103,	115, \
		70,	31,	37,	46,	56,	69,	85,	105,	117,	130, \
		75,	35,	43,	52,	64,	78,	97,	119,	133,	148, \
		80,	40,	49,	59,	73,	89,	110,	137,	152,	169, \
		85,	46,	56,	68,	83,	103,	127,	157,	175,	195, \
		90,	52,	64,	78,	96,	118,	146,	182,	203,	226, \
		95,	60,	73,	90,	111,	137,	169,	210,	233,	260, \
		100,	69,	85,	104,	128,	158,	196,	243,	271,	300, \
		105,	80,	99,	121,	148,	182,	226,	279,	311,	350, \
		110,	93,	115,	141,	172,	212,	261,	327,	365,	408, \
		115,	110,	135,	165,	203,	249,	307,	383,	429,	481, \
		120,	129,	159,	195,	240,	295,	366,	461,	516,	577



#define CA7_TABLE_2							\
	/* "(WAT -8.16%) Leakage Power"	 */                             \
	/**/		800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	8,	10,	12,	14,	18,	22,	27,	30,	34, \
		30,	9,	10,	13,	15,	19,	23,	29,	33,	37, \
		35,	9,	11,	14,	16,	20,	25,	32,	35,	40, \
		40,	10,	12,	15,	18,	22,	28,	35,	38,	43, \
		45,	11,	13,	16,	20,	25,	30,	38,	42,	47, \
		50,	12,	15,	18,	22,	27,	33,	41,	46,	52, \
		55,	14,	17,	20,	24,	30,	37,	46,	51,	57, \
		60,	15,	18,	22,	27,	33,	41,	51,	57,	64, \
		65,	17,	20,	25,	30,	36,	45,	56,	63,	70, \
		70,	19,	23,	27,	33,	41,	51,	63,	70,	79, \
		75,	21,	25,	31,	38,	47,	57,	71,	79,	88, \
		80,	24,	29,	35,	43,	52,	64,	79,	88,	99, \
		85,	27,	32,	39,	48,	59,	72,	91,	101,	113, \
		90,	31,	37,	45,	55,	67,	83,	103,	115,	128, \
		95,	35,	42,	52,	63,	77,	94,	116,	129,	145, \
		100,	40,	49,	58,	71,	87,	107,	133,	148,	165, \
		105,	46,	56,	68,	81,	99,	122,	151,	169,	190, \
		110,	53,	63,	77,	94,	114,	141,	176,	197,	221, \
		115,	61,	74,	90,	109,	134,	164,	206,	231,	259, \
		120,	71,	86,	105,	128,	158,	193,	241,	268,	299


#define GPU_TABLE_0							\
	/* "(WAT 11.57%) Leakage Power"	 */                             \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	22,	27,	35,	44,	57,	73,	94,	106,	121, \
		30,	25,	32,	40,	52,	65,	84,	108,	124,	140, \
		35,	29,	37,	47,	60,	76,	98,	126,	142,	160, \
		40,	34,	43,	54,	69,	88,	113,	145,	163,	183, \
		45,	39,	50,	64,	80,	102,	131,	166,	187,	210, \
		50,	46,	58,	74,	93,	118,	151,	190,	214,	241, \
		55,	54,	68,	86,	108,	137,	173,	219,	246,	277, \
		60,	63,	79,	100,	126,	158,	200,	252,	282,	317, \
		65,	74,	92,	116,	146,	183,	230,	288,	323,	363, \
		70,	86,	108,	135,	169,	211,	264,	331,	371,	416, \
		75,	100,	126,	157,	195,	244,	305,	382,	427,	478, \
		80,	118,	146,	182,	227,	283,	353,	440,	493,	553, \
		85,	138,	172,	213,	265,	330,	410,	511,	570,	637, \
		90,	162,	200,	247,	307,	382,	475,	590,	658,	733, \
		95,	188,	232,	287,	355,	441,	548,	681,	760,	847, \
		100,	218,	269,	333,	412,	509,	632,	783,	872,	972, \
		105,	255,	314,	387,	478,	590,	729,	901,	1002,	1114, \
		110,	297,	365,	450,	555,	683,	841,	1034,	1149,	1277, \
		115,	346,	425,	522,	643,	790,	970,	1191,	1318,	1464, \
		120,	405,	496,	608,	746,	914,	1118,	1370,	1516,	1679



#define GPU_TABLE_1							\
	/* "(WAT 0.78%) Leakage Power"	 */                             \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	8,	11,	13,	17,	22,	28,	35,	40,	46, \
		30,	9,	12,	15,	19,	24,	30,	39,	44,	50, \
		35,	10,	13,	17,	21,	26,	34,	43,	49,	55, \
		40,	12,	15,	19,	23,	30,	38,	48,	54,	61, \
		45,	14,	17,	21,	26,	33,	42,	53,	60,	67, \
		50,	15,	19,	24,	30,	37,	47,	59,	66,	74, \
		55,	18,	22,	27,	34,	42,	53,	66,	73,	82, \
		60,	20,	25,	31,	39,	48,	59,	73,	82,	92, \
		65,	23,	29,	36,	44,	53,	67,	82,	92,	103, \
		70,	27,	33,	40,	50,	61,	75,	93,	103,	116, \
		75,	31,	38,	47,	57,	70,	85,	105,	117,	131, \
		80,	37,	44,	53,	65,	79,	98,	120,	134,	150, \
		85,	42,	51,	62,	75,	92,	112,	138,	154,	171, \
		90,	49,	60,	72,	87,	106,	130,	159,	177,	196, \
		95,	58,	70,	84,	102,	123,	150,	184,	204,	227, \
		100,	69,	83,	100,	120,	144,	175,	214,	237,	263, \
		105,	82,	98,	118,	142,	170,	206,	251,	277,	307, \
		110,	97,	117,	140,	168,	202,	244,	296,	327,	361, \
		115,	117,	140,	167,	200,	240,	290,	351,	388,	429, \
		120,	141,	167,	199,	240,	287,	346,	420,	463,	511


#define GPU_TABLE_2							\
	/* "(WAT -8.16%) Leakage Power"	 */                             \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	7,	9,	12,	15,	19,	25,	32,	36,	42, \
		30,	8,	10,	12,	16,	20,	26,	34,	38,	44, \
		35,	8,	10,	13,	17,	22,	28,	36,	40,	46, \
		40,	9,	11,	14,	18,	23,	29,	38,	43,	49, \
		45,	10,	12,	16,	20,	25,	32,	41,	46,	52, \
		50,	11,	13,	17,	21,	27,	34,	44,	49,	56, \
		55,	12,	15,	18,	23,	29,	37,	47,	53,	60, \
		60,	13,	16,	20,	26,	32,	40,	51,	58,	65, \
		65,	15,	18,	23,	28,	35,	44,	56,	63,	71, \
		70,	16,	20,	25,	31,	39,	49,	62,	70,	78, \
		75,	19,	23,	28,	35,	44,	55,	69,	77,	87, \
		80,	21,	26,	32,	40,	49,	61,	77,	86,	97, \
		85,	24,	30,	37,	45,	56,	69,	86,	96,	108, \
		90,	28,	34,	42,	51,	63,	78,	97,	108,	121, \
		95,	32,	39,	48,	58,	72,	88,	109,	122,	136, \
		100,	36,	44,	54,	66,	81,	100,	123,	137,	153, \
		105,	42,	51,	62,	75,	92,	113,	140,	156,	173, \
		110,	48,	58,	71,	86,	106,	130,	159,	177,	197, \
		115,	56,	67,	82,	100,	122,	149,	183,	203,	225, \
		120,	64,	78,	94,	115,	140,	171,	210,	233,	259



#define VCORE_TABLE_0							\
	/* "(WAT 11.57%) Leakage Power" */                              \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	76,	93,	116,	145,	182,	228,	289,	325,	367, \
		30,	85,	106,	132,	165,	205,	259,	326,	365,	409, \
		35,	97,	121,	150,	184,	232,	290,	364,	410,	465, \
		40,	111,	137,	170,	212,	263,	329,	417,	470,	528, \
		45,	126,	156,	194,	240,	298,	377,	474,	531,	596, \
		50,	144,	178,	220,	273,	343,	428,	537,	602,	674, \
		55,	165,	204,	252,	314,	389,	485,	609,	682,	764, \
		60,	188,	233,	289,	357,	443,	554,	694,	778,	874, \
		65,	217,	267,	330,	408,	508,	634,	797,	894,	1003, \
		70,	249,	307,	379,	469,	585,	731,	916,	1025,	1152, \
		75,	288,	354,	437,	544,	676,	841,	1052,	1178,	1318, \
		80,	333,	409,	506,	628,	778,	968,	1209,	1352,	1511, \
		85,	388,	476,	586,	724,	897,	1115,	1395,	1560,	1743, \
		90,	454,	558,	683,	842,	1041,	1294,	1612,	1795,	2004, \
		95,	528,	648,	797,	981,	1210,	1498,	1863,	2079,	2320, \
		100,	617,	754,	926,	1141,	1403,	1735,	2154,	2402,	2680, \
		105,	722,	881,	1079,	1326,	1629,	2015,	2496,	2779,	3100, \
		110,	839,	1024,	1258,	1540,	1894,	2343,	2903,	3232,	3607, \
		115,	985,	1200,	1469,	1793,	2208,	2714,	3366,	3757,	4198, \
		120,	1147,	1407,	1718,	2104,	2575,	3156,	3911,	4369,	4880


#define VCORE_TABLE_1							\
	/* "(WAT 0.78%) Leakage Power" */                               \
	/**/            800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	33,	41,	50,	62,	78,	98,	124,	140,	159, \
		30,	36,	44,	54,	67,	84,	105,	132,	151,	172, \
		35,	39,	47,	59,	73,	91,	114,	142,	162,	182, \
		40,	42,	52,	64,	80,	100,	125,	158,	178,	199, \
		45,	47,	58,	71,	88,	110,	137,	172,	193,	218, \
		50,	53,	64,	80,	98,	122,	151,	189,	212,	238, \
		55,	59,	73,	89,	108,	134,	166,	207,	233,	261, \
		60,	67,	81,	99,	121,	149,	184,	230,	257,	288, \
		65,	75,	92,	111,	136,	167,	207,	256,	285,	317, \
		70,	86,	103,	126,	153,	189,	231,	285,	317,	355, \
		75,	98,	118,	143,	174,	212,	261,	321,	357,	402, \
		80,	112,	135,	163,	197,	239,	294,	364,	406,	451, \
		85,	129,	154,	185,	224,	273,	335,	413,	461,	515, \
		90,	148,	177,	213,	258,	314,	385,	475,	527,	588, \
		95,	172,	206,	247,	299,	364,	444,	544,	605,	674, \
		100,	201,	241,	289,	347,	421,	512,	627,	697,	775, \
		105,	236,	281,	336,	407,	492,	598,	731,	811,	906, \
		110,	276,	328,	393,	473,	572,	697,	853,	946,	1051, \
		115,	327,	388,	461,	553,	670,	815,	995,	1105,	1232, \
		120,	389,	460,	548,	656,	792,	963,	1174,	1293,	1442


#define VCORE_TABLE_2							\
	/* "(WAT -8.16%) Leakage Power" */                              \
	/**/	800,	850,	900,	950,	1000,	1050,	1100,	1125,	1150, \
		25,	31,	39,	49,	62,	80,	104,	135,	154,	176, \
		30,	32,	41,	52,	66,	84,	108,	140,	160,	183, \
		35,	35,	43,	55,	69,	88,	113,	147,	168,	193, \
		40,	37,	46,	58,	74,	94,	121,	155,	178,	203, \
		45,	40,	50,	63,	79,	100,	128,	166,	188,	214, \
		50,	44,	54,	68,	85,	108,	138,	177,	200,	228, \
		55,	48,	59,	74,	93,	117,	148,	190,	216,	245, \
		60,	52,	65,	81,	101,	127,	161,	205,	232,	263, \
		65,	57,	71,	88,	110,	139,	175,	223,	252,	286, \
		70,	64,	79,	98,	122,	152,	192,	244,	276,	312, \
		75,	72,	88,	109,	135,	168,	212,	267,	301,	340, \
		80,	80,	99,	122,	150,	187,	234,	295,	331,	374, \
		85,	90,	110,	136,	168,	210,	262,	328,	369,	414, \
		90,	103,	126,	155,	189,	235,	291,	365,	409,	460, \
		95,	117,	142,	174,	213,	262,	325,	405,	455,	511, \
		100,	133,	161,	196,	240,	295,	365,	454,	508,	569, \
		105,	149,	181,	220,	269,	332,	410,	509,	570,	638, \
		110,	169,	204,	249,	303,	372,	460,	570,	638,	713, \
		115,	193,	232,	280,	342,	420,	517,	642,	719,	803, \
		120,	221,	265,	321,	391,	479,	586,	727,	811,	907



typedef struct spower_raw_s {
	int vsize;
	int tsize;
	int table_size;
	int *table[];
} spower_raw_t;


/** table order: ff, tt, ss **/
int ca7_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	{CA7_TABLE_0},
	{CA7_TABLE_1},
	{CA7_TABLE_2},
};

int ca15l_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	{CA15L_TABLE_0},
	{CA15L_TABLE_1},
	{CA15L_TABLE_2},
};

int gpu_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	{GPU_TABLE_0},
	{GPU_TABLE_1},
	{GPU_TABLE_2},
};

int vcore_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	{VCORE_TABLE_0},
	{VCORE_TABLE_1},
	{VCORE_TABLE_2},
};


spower_raw_t ca7_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&ca7_data[0], (int *)&ca7_data[1], (int *)&ca7_data[2]},
};


spower_raw_t ca15l_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&ca15l_data[0], (int *)&ca15l_data[1], (int *)&ca15l_data[2]},
};

spower_raw_t gpu_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&gpu_data[0], (int *)&gpu_data[1], (int *)&gpu_data[2]},
};

spower_raw_t vcore_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&vcore_data[0], (int *)&vcore_data[1], (int *)&vcore_data[2]},
};



typedef struct voltage_row_s {
	int mV[VSIZE];
} vrow_t;

typedef struct temperature_row_s {
	int deg;
	int mA[VSIZE];
} trow_t;


typedef struct sptab_s {
	int vsize;
	int tsize;
	int *data;		/* array[VSIZE + TSIZE + (VSIZE*TSIZE)]; */
	vrow_t *vrow;		/* pointer to voltage row of data */
	trow_t *trow;		/* pointer to temperature row of data */
} sptbl_t;

#define trow(tab, ti)		((tab)->trow[ti])
#define mA(tab, vi, ti)	((tab)->trow[ti].mA[vi])
#define mV(tab, vi)		((tab)->vrow[0].mV[vi])
#define deg(tab, ti)		((tab)->trow[ti].deg)
#define vsize(tab)		((tab)->vsize)
#define tsize(tab)		((tab)->tsize)
#define tab_validate(tab)	(!!(tab) && (tab)->data != NULL)

static inline void spower_tab_construct(sptbl_t(*tab)[], spower_raw_t *raw)
{
	int i;
	sptbl_t *ptab = (sptbl_t *) tab;

	for (i = 0; i < raw->table_size; i++) {
		ptab->vsize = raw->vsize;
		ptab->tsize = raw->tsize;
		ptab->data = raw->table[i];
		ptab->vrow = (vrow_t *) ptab->data;
		ptab->trow = (trow_t *) (ptab->data + ptab->vsize);
		ptab++;
	}
}



#endif
