/*
 * main.c
 *
 *  Created on: Jul 21, 2016
 *      Author: shawn
 */


#include "main.h"

int isWAV(const char *filename)
{
	int len = strlen(filename);
	if (filename[len - 4] == '.'
			&& filename[len - 3] == 'w'
			&& filename[len - 2] == 'a'
			&& filename[len - 1] == 'v')
		return 1;
	return 0;
}

void set_outlist(char outlist[PATH_MAX + 1], const char *filename)
{
	int len = strlen(filename);
	strcpy(outlist, filename);
	strcpy(outlist + len - 3, "mp3");
}

int get_filelist(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int argc, char *argv[])
{
	DIR *pDir;
	struct dirent *pDirEnt;
	int nFileCnt = 0;

	if ((pDir = opendir(argv[1])) != NULL) {
		while ((pDirEnt = readdir(pDir)) != NULL) {
			if (pDirEnt->d_type == 8 && isWAV(pDirEnt->d_name)) {
				int len = strlen(argv[1]);
				strcpy(inlist[nFileCnt], argv[1]);
				if (argv[1][len - 1] != '/')
					strcat(inlist[nFileCnt], "/");
				strcat(inlist[nFileCnt], pDirEnt->d_name);

				set_outlist(outlist[nFileCnt], inlist[nFileCnt]);
				nFileCnt++;
			}
		}
		closedir(pDir);
	}
	else {
		if (isWAV(argv[1])) {
			strcpy(inlist[nFileCnt], argv[1]);
			if (argc == 3)
				strcpy(outlist[nFileCnt], argv[2]);
			else
				set_outlist(outlist[nFileCnt], argv[1]);
			nFileCnt++;
		}
	}

	return nFileCnt;
}

static FILE * init_file(lame_t gf, const char *inFile, char *outFile, int nFile)
{
	FILE *outf;

	if (strcmp(inFile, outFile) == 0) {
		fprintf(stderr, "ERROR: The input file name is same with output file name. Abort.\n");
		return NULL;
	}

	if (!isWAV(inFile)) {
		fprintf(stderr, "ERROR: Input file is not wav file.\n");
		return NULL;
	}

	if (init_infile(gf, inFile, nFile) < 0) {
		fprintf(stderr, "ERROR: Initializing input file failed.\n");
		return NULL;
	}

	if ((outf = init_outfile(outFile)) == NULL) {
		fprintf(stderr, "ERROR: Initializing output file failed.\n");
		return NULL;
	}

	return outf;
}

void usage()
{
	printf("Too few arguments. See below usage:\n"
			"\tMP3Enc <input_filename [output_filename] | input_directory>\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	char inFileList[NAME_MAX][PATH_MAX + 1];
	char outFileList[NAME_MAX][PATH_MAX + 1];

	FILE *outf[NAME_MAX];
	lame_t gf[NAME_MAX];
	int nFiles;
	int ret;

	printf("MP3Enc v" VERSION "\n");

	if (argc < 2)
		usage();

	nFiles = get_filelist(inFileList, outFileList, argc, argv);
	if (nFiles < 1) {
		fprintf(stderr, "No files to encoding.\n");
		return -1;
	}


	for (int i = 0; i < nFiles; i++) {

		gf[i] = lame_init();

		/* turn off automatic writing of ID3 tag data into mp3 stream
			 * we have to call it before 'lame_init_params', because that
			 * function would spit out ID3v2 tag data.
			 */
		lame_set_write_id3tag_automatic(gf[i], 0);
		if (lame_init_params(gf[i]) < 0) {
			fprintf(stderr, "lame_init_params error\n");
		}

		if ((outf[i] = init_file(gf[i], inFileList[i], outFileList[i], i)) == NULL) {
			fprintf(stderr, "ERROR: init_file failed (#%d, %s)\n", i, inFileList[i]);
			return -1;
		}
	}
	printf("init_file succeeded\n");

	pthread_t *tid = malloc(sizeof(pthread_t) * nFiles);
	th_param_t *params = malloc(sizeof(th_param_t) * nFiles);
	for (int i = 0; i < nFiles; i++) {
		(params + i)->gf = gf[i];
		(params + i)->outf = outf[i];
		(params + i)->inPath = inFileList[i];
		(params + i)->outPath = outFileList[i];
		(params + i)->nFile = i;

		printf("Thread %d creates\n", i);
		printf(" %s, %s, #%02d\n", (params + i)->inPath, (params + i)->outPath, (params + i)->nFile);

		pthread_create(&tid[i], NULL, lame_encoder_loop, (void *)(params + i));
		lame_init_bitstream(gf[i]);
	}

	for	(int i = 0; i < nFiles; i++) {
		printf("Thread %d joins\n", i);
		pthread_join(tid[i], (void **)&ret);
	}
	if (ret)
		fprintf(stderr, "ERROR: Encoding is failed\n");

	for	(int j = 0; j < nFiles; j++) {
		fclose(outf[j]);
		close_infile(j);
	}

	return 0;
}
