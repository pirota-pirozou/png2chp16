#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<setjmp.h>
#ifdef __NEWLIB__
#include	<fcntl.h>
#include	<unistd.h>
#endif

#include	"png.h"
#include	"pngctrl.h"

#define PNG_BYTES_TO_CHECK 4

#ifdef __cplusplus
extern "C" {
#endif

	// png�֘A
	static unsigned char * pngstream_ptr = NULL;

	static int				png_trans_num;				// �����F�̐�
	static png_color_16		png_trans_col;				// �����F
	static png_colorp		palette;                    // �p���b�g



	// png�ǂݍ��݃X�g���[��
	static void libReadStream(png_structp png_ptr, png_bytep data, png_size_t length)
	{
		memcpy(data, pngstream_ptr, length);

		pngstream_ptr += length;
	}

	// png�X�g���[��������
	static void libInitStream(void *pptr)
	{
		pngstream_ptr = (u_char *)pptr;
	}

	// �r�b�g�}�b�v2�����z��m��
	png_bytepp alloc_map(IMAGEDATA* img)
	{
		int width = img->width;
		int height = img->height;
		int i;

		img->map = (png_bytepp) malloc(sizeof(png_bytep) * height);
		if (img->map == NULL)
		{
			return NULL;
		}

		for (i = 0; i < height; i++)
		{
			img->map[i] = (png_bytep) malloc(sizeof(png_byte) * width);
			if (img->map[i] == NULL)
			{
				return NULL;
			}
		}

		return img->map;
	}

	// �r�b�g�}�b�v2�����z����
	void free_map(IMAGEDATA* img)
	{
		int height = img->height;
		int i;
		if (img->map != NULL)
		{
			for (i = 0; i < height; i++)
			{
				free(img->map[i]);
				img->map[i] = NULL;
			}
			free(img->map);
			img->map = NULL;
		}

	}

	// png���������݁i�C���f�b�N�X�J���[��p�j
	int writepng(const char *filename, IMAGEDATA *img)
	{
		int result = -1;
		FILE *fp;

		if (img == NULL) {
			return result;
		}
		fp = fopen(filename, "wb");
		if (fp == NULL) {
			perror(filename);
			return result;
		}
		result = write_png_stream(fp, img);
		fclose(fp);

		return 0;
	}

	// �r�b�g�}�b�v�ƃp���b�g��png�Ƃ��ăZ�[�u
	int write_png_stream(FILE* fp, IMAGEDATA *img)
	{
		int i, x, y;
		int result = -1;
		int row_size;
		int color_type;
		png_structp png = NULL;
		png_infop info = NULL;
		png_bytep row;
		png_bytepp rows = NULL;
		png_colorp palette = NULL;
		if (img == NULL) {
			return result;
		}
		switch (img->color_type) {
		case COLOR_TYPE_INDEX:  // �C���f�b�N�X�J���[
			color_type = PNG_COLOR_TYPE_PALETTE;
			row_size = sizeof(png_byte) * img->width;
			break;
		default:
			return result;
		}
		png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png == NULL) {
			goto error;
		}
		info = png_create_info_struct(png);
		if (info == NULL) {
			goto error;
		}
		if (setjmp(png_jmpbuf(png))) {
			goto error;
		}
		png_init_io(png, fp);
		png_set_IHDR(png, info, img->width, img->height, 8,
			color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
		rows = png_malloc(png, sizeof(png_bytep) * img->height);
		if (rows == NULL) {
			goto error;
		}
		png_set_rows(png, info, rows);
		memset(rows, 0, sizeof(png_bytep) * img->height);
		for (y = 0; y < img->height; y++) {
			if ((rows[y] = png_malloc(png, row_size)) == NULL) {
				goto error;
			}
		}
		switch (img->color_type) {
		case COLOR_TYPE_INDEX:  // �C���f�b�N�X�J���[
			palette = png_malloc(png, sizeof(png_color) * img->palette_num);
			for (i = 0; i < img->palette_num; i++) {
				palette[i].red = img->palette[i].r;
				palette[i].green = img->palette[i].g;
				palette[i].blue = img->palette[i].b;
			}
			png_set_PLTE(png, info, palette, img->palette_num);
			for (i = img->palette_num - 1; i >= 0 && img->palette[i].a != 0xFF; i--);
			if (i >= 0) {
				int num_trans = i + 1;
				png_byte trans[256];
				for (i = 0; i < num_trans; i++) {
					trans[i] = img->palette[i].a;
				}
				png_set_tRNS(png, info, trans, num_trans, NULL);
			}
			png_free(png, palette);
			for (y = 0; y < img->height; y++) {
				row = rows[y];
				for (x = 0; x < img->width; x++) {
					*row++ = img->map[y][x];
				}
			}
			break;

		default:;
		}
		png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
		result = 0;
	error:
		if (rows != NULL) {
			for (y = 0; y < img->height; y++) {
				png_free(png, rows[y]);
			}
			png_free(png, rows);
		}
		png_destroy_write_struct(&png, &info);
		return result;
	}


	// png���J��DIB�ɕϊ�
	PDIB pngptr2dib(u_char* pptr)
	{
		png_structp     png_ptr;
		png_infop       info_ptr;
		unsigned long   width, height;
		int             bit_depth, color_type, interlace_type;
		png_bytepp		image;
		png_bytepp		imagetmp;

		int				i;
		int				y;
		LPBITMAPINFO	dib_ptr = NULL;
		png_uint_32		ulRowBytes;
		png_uint_32		ulUnitBytes;
		RGBQUAD*		pcolors;
		png_bytep		src;
		png_bytep		dest;
		png_byte		c;

		unsigned char*	dibp;
		int				alc_sz;
		unsigned int	num_pal;

		png_bytep		pal_trns_p = NULL;
		int				num_trns;

		png_color_16p	trans_values = NULL;

		// �t�@�C����PNG���H
		if (png_sig_cmp((u_char*)pptr, (png_size_t)0, PNG_BYTES_TO_CHECK))
			return NULL;

		//
		png_ptr = png_create_read_struct(							// png_ptr�\���̂��m�ہE������
			PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL)
		{
			return NULL;
		}

		info_ptr = png_create_info_struct(png_ptr);					// info_ptr�\���̂��m�ہE������
		if (info_ptr == NULL)
		{
			png_destroy_read_struct(&png_ptr, NULL, NULL);
			return NULL;
		}

		libInitStream(pptr);										// �X�g���[����������
		png_set_read_fn(png_ptr, NULL, libReadStream);				// libpng�ɃX�g���[��������ݒ�

		png_read_info(png_ptr, info_ptr);			               // PNG�t�@�C���̃w�b�_��ǂݍ���
		png_get_IHDR(png_ptr, info_ptr, &width, &height,	       // IHDR�`�����N�����擾
			&bit_depth, &color_type, &interlace_type,
			NULL, NULL);

		if (color_type == PNG_COLOR_TYPE_PALETTE)
		{
			png_get_PLTE(png_ptr, info_ptr, &palette, &num_pal);            // �p���b�g�擾
		}
		else
		{
			num_pal = 0;
		}
		/*
			if (color_type == PNG_COLOR_TYPE_PALETTE)
			{
				png_set_palette_to_rgb(png_ptr);	// �����TRNS�`�����N���W�J����A1dot=RGBA�ɓW�J����Ă��܂�
			}
		//    png_set_strip_alpha(png_ptr);			// �����ALPHA��W�J���Ȃ��čς�

			// tell libpng to strip 16 bit/color files down to 8 bits/color
			if (bit_depth == 16)
			   png_set_strip_16(png_ptr);

			if (color_type == PNG_COLOR_TYPE_PALETTE || bit_depth < 8)
				png_set_expand(png_ptr);

			// after the transformations have been registered update info_ptr data
			png_read_update_info(png_ptr, info_ptr);

			// get again width, height and the new bit-depth and color-type
			png_get_IHDR(png_ptr, info_ptr, &width, &height,
							&bit_depth, &color_type, &interlace_type,
							NULL, NULL);

			png_set_bgr(png_ptr);
		  */


		  // �`���̏�����
		  //   �摜�T�C�Y = width, height
		  //   �F�� = bit_depth
		  //   �ق�

		  // ���P���C���̃o�C�g��
		ulRowBytes = png_get_rowbytes(png_ptr, info_ptr);
		// �P�h�b�g�̃o�C�g��
		ulUnitBytes = ulRowBytes / width;

		// ����4dot�̔{���ɂȂ�悤��
		if (width & 3)
		{
			i = (4 - (width & 3));
			width += i;
			ulRowBytes += i * ulUnitBytes;
		}

		// 16�F(Packed)�̏ꍇ�Ƀp�b�`����
		if (ulUnitBytes == 0)
		{
			ulRowBytes *= 2;
		}

		// �m�ۃT�C�Y
		alc_sz = sizeof(png_byte) * ulRowBytes * height + sizeof(BITMAPINFOHEADER) +
			sizeof(RGBQUAD) * num_pal;

		// �r�b�g�}�b�v���m��
		dib_ptr = malloc(alc_sz);

		// BITMAPINFOHEADER ���N���A
		memset((BITMAPINFOHEADER*)dib_ptr, 0, sizeof(BITMAPINFOHEADER));

		// ���ߐF�`�F�b�N
		png_trans_num = 0;
		if (png_get_tRNS(png_ptr, info_ptr, NULL, &num_trns, &trans_values))
		{
			png_trans_num = num_trns;					// �����F�̐�
			png_trans_col = *trans_values;				// �����F
		}

		// �w�b�_�ɏ�������
		dib_ptr->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		dib_ptr->bmiHeader.biBitCount = (color_type == PNG_COLOR_TYPE_PALETTE) ? 8 : 24;
		dib_ptr->bmiHeader.biWidth = width;
		dib_ptr->bmiHeader.biHeight = height;
		dib_ptr->bmiHeader.biPlanes = 1;
		dib_ptr->bmiHeader.biCompression = BI_RGB;
		dib_ptr->bmiHeader.biSizeImage = (ulRowBytes * height);
		dib_ptr->bmiHeader.biClrUsed = num_pal;

		//
		dibp = (u_char*)dib_ptr + sizeof(BITMAPINFOHEADER);

		if (dib_ptr->bmiHeader.biCompression != BI_BITFIELDS)
		{
			pcolors = (RGBQUAD*)dibp;                                     // RGBQUAD�ւ̃|�C���^
			dibp =(u_char*)dibp + (sizeof(RGBQUAD) * dib_ptr->bmiHeader.biClrUsed);
		}
		else
		{
			pcolors = NULL;
			dibp = (void*)((u_char*)dibp + 3);
		}

		// �p���b�g�R�s�[
		if (color_type == PNG_COLOR_TYPE_PALETTE)
		{
			for (i = 0; i < num_pal; i++)
			{
				pcolors->rgbRed = palette->red;
				pcolors->rgbGreen = palette->green;
				pcolors->rgbBlue = palette->blue;
				pcolors->rgbReserved = 0;
				palette++;
				pcolors++;
			}
		}

		// �W�J�|�C���^�z����m��
		image = (png_bytepp) malloc(height * sizeof(png_bytep));

		// 16�F(Packed)�ȊO�̏ꍇ�i�ʏ�j
		if (ulUnitBytes != 0)
		{
			for (i = 0; i < height; i++) {
				image[i] = (png_bytep) dibp + (i * ulRowBytes);
			}
			png_read_image(png_ptr, image);							// �摜�f�[�^�̓W�J
		}
		else
		{
			// 16�F(Packed)�̏ꍇ
			imagetmp = malloc(height * width / 2);

			for (i = 0; i < height; i++) {
				image[i] = (png_bytep) imagetmp + (i * (width / 2));
			}
			png_read_image(png_ptr, image);							// �摜�f�[�^�̓W�J

			// 4bit �� 8bit�W�J
			for (y = 0; y < height; y++)
			{
				src = (png_bytep) image[y];
				dest = (png_bytep) dibp + (y * ulRowBytes);
				for (i = 0; i < width; i += 2)
				{
					c = (png_byte) (*src);
					*dest = (c >> 4);
					*(dest + 1) = (c & 0x0F);
					src++;
					dest += 2;
				}
			}
			free(imagetmp);
		}
		free(image);

		png_destroy_read_struct(								// libpng�\���̂̃��������
			&png_ptr, &info_ptr, (png_infopp)NULL);

		return (PDIB)dib_ptr;
	}

	// PNGOpenFile
	PDIB PngOpenFile(const char* szFile)
	{
		PDIB pdib = NULL;
		u_char* png = NULL;

		size_t fsize;
		size_t rbytes;

		FILE* fp;

		// �t�@�C������PNG�f�[�^���w���|�C���^���H
		if (!png_sig_cmp((const u_char*)szFile, (png_size_t)0, PNG_BYTES_TO_CHECK))
		{
			printf("PngOpenFile: png_sig_cmp(is Pointer)\n");
			// ���������������璼�ړW�J
			pdib = pngptr2dib((u_char*)szFile);

			return pdib;
		}

		printf("PngOpenFile(%s)\n", szFile);
		// PNG�t�@�C�����ۂ��ƊJ���ă������m��
		fp = fopen(szFile, "rb");
		if (fp == NULL)
		{
			printf("PngOpenFile: fopen(%s) failed\n", szFile);
			// �J���Ȃ�
			return NULL;
		}
		fseek(fp, 0L, SEEK_END);							// �t�@�C���T�C�Y�̎擾
		fsize = ftell(fp); 	        						// get the current file pointer
		fseek(fp, 0L, SEEK_SET);							// �t�@�C���̐擪��

		// �������m��
		png = (u_char*)malloc(fsize);
		if (png == NULL)
		{
			printf("PngOpenFile: malloc(%d) failed\n", (int)fsize);
			fclose(fp);
			return NULL;
		}

		// �ǂݍ���
		rbytes = fread(png, 1, fsize, fp);
		fclose(fp);

		// �S���ǂ߂Ȃ������H
		if (fsize != rbytes)
		{
			printf("PngOpenFile: fread(%d) failed\n", (int)fsize);
			return NULL;
		}

		// PNG����DIB�ɓW�J
		pdib = pngptr2dib(png);

		// PNG�C���[�W�J��
		free(png);

		return pdib;
	}

#ifdef __cplusplus
}	// extern "C"
#endif