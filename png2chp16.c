/////////////////////////////////////////////////////////////////////////////
// png2chp16.c
//
// PNG to X68 Sprite Pattern Converter
// programmed by pirota
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __NEWLIB__
#include <fcntl.h>
#include <unistd.h>
#endif

#include "png.h"
#include "pngctrl.h"

#define INFILE_EXT  ".png"
#define OUTFILE_EXT ".CHP"
#define OUTFILEPAT_EXT ".PAT"
#define OUTFILEPAL_EXT ".PAL"

#define _MAX_DRIVE	3
#define _MAX_DIR	256
#define _MAX_FNAME	256
#define _MAX_EXT	256

static u_short pal[256];										// X68Kパレット

PDIB dibbuf = NULL;
u_char *patbuf = NULL;

int outsize;											// 出力サイズ

static char infilename[256];
static char outfilename[256];
static char patfilename[256];
static char palfilename[256];

static int main_result = 0;

static int opt_p = 0;												// べた書きオプション

static int readjob(void);
static int cvjob(void);

// 使用法の表示
static void usage(void)
{
    printf("usage: png2chp16 infile[" INFILE_EXT "] OutFile\n"\
		   "\t-p\tスプライトパターン(PAT)/パレット(PAL)ベタ出力\n"
		   );

    exit(0);
}


/// @brief basename
/// @param path
/// @return
char* basename(char *path)
{
	char *p;
	if( path == NULL || *path == '\0' )
		return ".";
	p = path + strlen(path) - 1;
	while( *p == '/' ) {
		if( p == path )
			return path;
		*p-- = '\0';
	}
	while( p >= path && *p != '/' )
		p--;
	return p + 1;
}

/// @brief dirname
/// @param path
/// @return
char* dirname(char *path)
{
	char *p;
	if( path == NULL || *path == '\0' )
		return ".";
	p = path + strlen(path) - 1;
	while( *p == '/' ) {
		if( p == path )
			return path;
		*p-- = '\0';
	}
	while( p >= path && *p != '/' )
		p--;
	if( p < path )
		return ".";
	*p = '\0';
	return path;
}

/// @brief パス名を分解する
/// @param path
/// @param drive
/// @param dir
/// @param name
/// @param ext
void splitpath(const char *path, char *drive, char *dir, char *name, char *ext)
{
    char *basec, *bname, *dirc, *dname;
    char *lastdot;
    char *extc;

    /* Copy path */
#ifdef _MSC_VER
    basec = _strdup(path);
    dirc = _strdup(path);
    extc = _strdup(path);
#else
	basec = strdup(path);
	dirc = strdup(path);
	extc = strdup(path);
#endif

    /* Get file name and directory */
    bname = basename(basec);
    dname = dirname(dirc);

    /* Get the last dot position */
    lastdot = strrchr(bname, '.');

    /* Split the file name and extension */
    if (lastdot != NULL) {
        *lastdot = '\0';
        strcpy(ext, lastdot + 1);
    } else {
        ext[0] = '\0';
    }

    strcpy(name, bname);
    strcpy(dir, dname);
    drive[0] = '\0';

    free(basec);
    free(dirc);
    free(extc);
}

/////////////////////////////////////
// main
/////////////////////////////////////
int main(int argc, char *argv[])
{
	int i;
	int ch;

	char drive[_MAX_DRIVE] = { 0 };
	char dir[_MAX_DIR] = { 0 };
	char fname[_MAX_FNAME] = { 0 };
	char ext[_MAX_EXT] = { 0 };

#ifdef ALLMEM
	allmem();
#endif

	// コマンドライン解析
	memset(fname, 0, sizeof(fname));
	memset(infilename, 0, sizeof(infilename));
	memset(outfilename, 0, sizeof(outfilename) );
	memset(patfilename, 0, sizeof(patfilename) );
	memset(palfilename, 0, sizeof(palfilename) );

    printf("PNG to 16x16Chip(256) Converter Ver0.00 " __DATE__ "," __TIME__ " Programmed by Pirota\n");

    if (argc <= 1)
	{
		usage();
	}


	for (i=1; i<argc; i++)
	{
		ch = argv[i][0];
		if (ch == '-' || ch == '/')
		{
			// スイッチ
			switch (argv[i][1])
			{
			case 'p':
				opt_p = 1;
				break;
			default:
				printf("-%c オプションが違います。\n",argv[i][1]);
				break;
			}

			continue;
		}
		// ファイル名入力
		if (!infilename[0])
		{
			strcpy(infilename, argv[i]);
			splitpath(infilename , drive, dir, fname, ext );
			if (ext[0]==0)
				strcat(infilename, INFILE_EXT);							// 拡張子補完

			continue;
		}

		// 出力ファイル名の作成
		if (!outfilename[0])
		{
			// 出力ファイルネーム
			strcpy(outfilename, argv[i]);
		}

	}
	// 出力ファイル名が省略されてたら
	if (!outfilename[0])
	{
		// 出力ファイルネーム
		strcpy(outfilename, fname);
	}

	splitpath(outfilename, drive, dir, fname, ext);
	if (ext[0] == 0)
		strcat(outfilename, OUTFILE_EXT);							// 拡張子補完

	// べた書きファイルネーム
	splitpath(outfilename , drive, dir, fname, ext );
	strcpy(patfilename, fname);
	strcat(patfilename, OUTFILEPAT_EXT);							// 拡張子補完

	strcpy(palfilename, fname);
	strcat(palfilename, OUTFILEPAL_EXT);							// 拡張子補完

    // ファイル読み込み処理
    if (readjob()<0)
		goto cvEnd;


	// 出力バッファの確保
	outsize = dibbuf->biWidth * dibbuf->biHeight;
	patbuf =  malloc(outsize);
	if (patbuf == NULL)
	{
		printf("出力バッファは確保できません\n");
		goto cvEnd;
	}
	memset(patbuf, 0, outsize);

    // 変換処理
	if (cvjob() < 0)
	{
		goto cvEnd;
	}

cvEnd:
	// 後始末
	// PX2出力バッファ開放
	if (patbuf != NULL)
	{
		free(patbuf);
	}

	// オフスクリーン開放
	if (dibbuf != NULL)
	{
		free(dibbuf);
	}

	return main_result;
}

//----------------------------
// ＰＮＧ読み込みＤＩＢに変換
//----------------------------
// Out: 0=ＯＫ
//      -1 = エラー
static int readjob(void)
{
	dibbuf = PngOpenFile((const char*)infilename);
	if (dibbuf == NULL)
	{
		printf("Can't open '%s'.\n", infilename);
		return -1;
	}

	if (dibbuf->biBitCount != 8)
	{
		printf("'%s' is not 8bit.\n", infilename);
		return -1;
	}

	return 0;
}


///////////////////////////////
// パターンデータに変換処理
///////////////////////////////
static int cvjob(void)
{
	BITMAPINFOHEADER* bi;
	size_t a;
	u_char* pimg;
	u_char* outptr;
	FILE* fp;
	int x, y;
	int xl, yl;
	RGBQUAD* paltmp;
	WORD x68pal;
	u_char dot;
	int err = 0;
	RGBQUAD* dibpal;

	bi = (BITMAPINFOHEADER*)dibbuf;

	// パターン変換処理
	outptr = patbuf;										// チップパターン出力バッファ

	pimg = NULL;
	for (yl = 0; yl < bi->biHeight; yl += 16)
	{
		for (xl = 0; xl < bi->biWidth; xl += 16)
		{
			// ピクセル変換
			for (y = 0; y < 16; y++)
			{
				pimg = (u_char*)dibbuf + (sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * bi->biClrUsed) + ((yl + y) * bi->biWidth) + xl;
				for (x = 0; x < 16; x++)
				{
					dot = (*pimg++);
					*(outptr++) = dot;
				} // x
			} // y
		} // xl

	} // yl

	// パレット変換
	dibpal = (RGBQUAD*)((u_char*)dibbuf + sizeof(BITMAPINFOHEADER));
	// GGGGG_RRRRR_BBBBB_I
	for (x=0; x<256; x++)
	{
		paltmp = &dibpal[x];
		x68pal = ((paltmp->rgbGreen >> 3) << 11) | ((paltmp->rgbRed >> 3) << 6) | ((paltmp->rgbBlue >> 3) << 1);
#ifdef BIG_ENDIAN
		pal[x] = x68pal;
#else
		pal[x] = (x68pal>>8)|((x68pal & 0xFF) << 8);			// エンディアン変換
#endif
	}

	// ベタファイル出力
	fp = fopen(patfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", patfilename);
		return -1;
	}

	// PATパターン出力
	err = 0;
	a = fwrite(patbuf, 1, outsize, fp);
	if (a != outsize)
	{
		printf("'%s' ファイルが正しく書き込めませんでした！\n", patfilename);
		err++;
	}

	fclose(fp);

	// 結果出力
	if (err == 0)
	{
		printf("パターンデータ '%s'を作成しました。\n", patfilename);
	}
	else
	{
		return -1;
	}

	fp = fopen(palfilename, "wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", palfilename);
		return -1;
	}

	// PAL出力
	err = 0;
	a = fwrite(pal, 1, 512, fp);
	if (a != 512)
	{
		printf("'%s' ファイルが正しく書き込めませんでした！\n", palfilename);
		err++;
	}

	fclose(fp);

	// 結果出力
	if (err == 0)
	{
		printf("パレットデータ '%s'を作成しました。\n", palfilename);
	}
	else
	{
		return -1;
	}


	return 0;
}
