#ifndef _IMGHEADER_H_
#define _IMGHEADER_H_



struct partition_info {
    char dummy[8];
    char name[32];
    uint32_t start_addr;
    uint32_t actual_size;
    uint32_t start_sector;
    uint32_t unknown;
    uint32_t flags;
    uint32_t part_size;
};

typedef struct  {
    unsigned char magic[4];   
    unsigned int codeword;       
    unsigned int image_id;      
    unsigned int image_src;       
    unsigned int image_dest_ptr;       
    unsigned int image_size;      
    unsigned int code_size;       
    unsigned int sig_ptr;       
    unsigned int sig_size;     
    unsigned int cert_chain_ptr;       
    unsigned int cert_chain_size;       
    unsigned int oem_root_cert_sel;       
    unsigned int oem_num_root_certs;
}sbl_hdr;

typedef struct  {
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
}mbn_hdr;

typedef struct  {
    unsigned int magic; /* 0xdeadbeef */
    unsigned int totalsize;
    unsigned int off_dt_struct;
    unsigned int off_dt_strings;
    unsigned int off_mem_rsvmap;         
    unsigned int version;
    unsigned int last_comp_version;
    unsigned int boot_cpuid_phys;
    unsigned int size_dt_strings;
    unsigned int size_dt_struct;
}dtb_hdr;

#endif
