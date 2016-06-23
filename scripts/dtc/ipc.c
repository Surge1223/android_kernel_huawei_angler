#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include "imgheader.h"

typedef unsigned char BYTE;

void *parse_mbn(const char *infname, char *_insize)
{
    char *image;
    int insize;
    FILE *fd;
    image = 0;
    dtb_hdr *header;
    sbl_hdr *sbl;
    size_t header_size;
    int in_fd = open(infname, O_RDONLY);
    const int cbuf_size = (1 << 20);
    char fname_buf[128];

    char *header_buf, *content_buf;

    fd = fopen(infname, "rb");
	if (fd==NULL)
	{
        printf ("Cannot open input file!\n");
        return;
	};    
  //  if((fd, 0, SEEK_END) != 0) goto oops;
    
    insize = ftell(fd); // number of bytes from the beginning of the file.
    if(insize < 0) {
printf ("Cannot open input file!\n");
 goto oops;
   } 
    if(fseek(fd, 0, SEEK_SET) != 0) {
    printf ("Cannot open input file!\n");
 goto oops;
 }  
    header_buf = malloc(0x4);
    if (!header_buf) return;
    lseek(in_fd, 4, SEEK_END);
    read(in_fd, header_buf, 0x4);
    header_size = sizeof(dtb_hdr);
	   header = (dtb_hdr*)        
malloc(header_size);

	  if(fread(header, 1, header_size, fd) != header_size) {
		printf("Error: cannot read img header !\n");
		return -1;
	} else {

    printf("image_dest_ptr 0x%d\n", header->totalsize);
    printf("image_id :%s\n", header->magic);
    printf("version :%d\n",header->off_dt_struct);

 
   } 
    /*
    unsigned char magic[4];   
    unsigned int image_id;       
    unsigned int header_vsn_number;       
    unsigned int image_src;       
    unsigned int image_dest_ptr;       
    unsigned int image_size;      
    unsigned int code_size;       
    unsigned int sig_ptr;       
    unsigned int sig_size;     
    unsigned int cert_chain_ptr;       
    unsigned int cert_chain_size;       
    image = (char*) malloc(0x4);
*/
    if(image == 0) goto oops;
    
    if(fread(image, 1, insize, fd) != insize) goto oops;
    fclose(fd);
    
    if(_insize) *_insize = insize;
    return;

oops:
    fclose(fd);
    if(image != 0) free(image);
	printf ("SHIT\n");
    return;

}

//memcpy (&real_flen, buf+3, 4);
//decrypt (buf+(3+4), flen-(3+4), pw);


int main(int argc, char** argv) {
    char *infname, *outfname;
    if (argc != 3) {
        printf("Usage: %s input.mbn output_dir\n", argv[0]);
        exit(0);
    }
    infname = argv[1];
    outfname = argv[2];

    parse_mbn(infname, outfname);
    return 0;
}
