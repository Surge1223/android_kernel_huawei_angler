
/**
 *  unpack_bootldr!_img : tool to unpack Android boot images
 *  https://gist.github.com/frederic/
 **/
/**
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* msm8960 bootloader.img format */
#define BOOTLDR_MAGIC "BOOTLDR!"
#define BOOTLDR_MAGIC_SIZE 8

typedef struct {
	char name[64];
	uint32_t size;
}img_info_s;

typedef struct {
	char magic[BOOTLDR_MAGIC_SIZE];
	uint32_t num_images;
	uint32_t start_offset;
	uint32_t bootldr_size;
	img_info_s img_info[];
}bootloader_images_header;

int main(int argc, char *argv[]){
	FILE *img_file, *out_file;
	bootloader_images_header *header;
	size_t result, img_info_size, img_size, header_size;
	int i;
	char *buffer, *img_name;

	if(argc < 2){
		int i;
		printf("Unpack msm8960 image file (magic BOOTLDR!)\n");
		printf("Usage: %s <img_filename>\n", argv[0]);
		return -1;
	}

	img_file = fopen(argv[1], "r");
	if(!img_file) {
		printf("Error opening img file!\n");
		return -1;
	}

	header_size = sizeof(bootloader_images_header);
	header = (bootloader_images_header*) malloc(header_size);

	result = fread(header, 1, header_size, img_file);
	if (result != header_size) {
		printf("Error: cannot read img header !\n");
		return -1;
	}

	if(strncmp(header->magic, BOOTLDR_MAGIC, BOOTLDR_MAGIC_SIZE)) {
		printf("Error: wrong magic value in header!\n");
		return -1;
	}

	img_info_size = sizeof(img_info_s) * header->num_images;
	header_size += img_info_size;
	header = realloc(header, header_size);
	result = fread(header->img_info, 1, img_info_size, img_file);
	if (result != img_info_size) {
		printf("Error: cannot read img infos !\n");
		return -1;
	}

	fseek (img_file, header->start_offset, SEEK_SET);

	for(i = 0; i < header->num_images; i++){
		img_size = header->img_info[i].size;
		img_name = header->img_info[i].name;
		printf("#%i %s (%i bytes)\n", i, img_name, img_size);
		buffer = (char*) malloc(img_size);
		result = fread(buffer, 1, img_size, img_file);
		if (result != img_size) {
			printf("Error: cannot read enough bytes from img file !\n");
			return -1;
		}
		out_file = fopen(img_name, "wb");
		if(!out_file) {
			printf("Error opening out img file (%s)!\n", img_name);
			return -1;
		}

		result = fwrite (buffer, 1, img_size, out_file);
		if(result != img_size)
			printf("Warning : out img file has not been fully written!\n");

		fclose(out_file);
		free(buffer);
	}

	free(header);
	fclose(img_file);
	return 0;
}
