#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int _concat_internal(char **dst, const char *src, va_list args){
    if(!src || !dst)
        return 0;

    size_t srclen = strlen(src), dstlen = 0;

    if(*dst)
        dstlen = strlen(*dst);

    va_list args1;
    va_copy(args1, args);

    size_t total = srclen + dstlen + vsnprintf(NULL, 0, src, args) + 1;

    char *dst1 = malloc(total);

    if(!(*dst))
        *dst1 = '\0';
    else{
        strncpy(dst1, *dst, dstlen + 1);
        free(*dst);
        *dst = NULL;
    }

    int w = vsnprintf(dst1 + dstlen, total, src, args1);

    va_end(args1);

    *dst = realloc(dst1, strlen(dst1) + 1);

    return w;
}

static int concat(char **dst, const char *src, ...){
    va_list args;
    va_start(args, src);

    int w = _concat_internal(dst, src, args);

    va_end(args);

    return w;
}

static void help(void){
    printf("simplefuzzer usage\n");
    printf("    -b          use this byte for changes instead of arc4random (optional)\n");
    printf("    -e          don't touch anything after this file offset (optional)\n");
    printf("    -f          input file name\n");
    printf("    -n          number of bytes to change in input file (default: 1)\n");
    printf("    -o          fuzzed output file name (default: inputfile-fuzzed-date)\n");
    printf("    -s          don't touch anything before this file offset (optional)\n");
    printf("    -u          unique: don't change the same byte more than once (optional)\n");
}

int main(int argc, char **argv){
    if(argc < 2){
        help();
        return 1;
    }

    int b_switch = 0;
    char user_fuzz_byte = 0;

    int e_switch = 0;
    size_t user_end_offset = 0;

    char *inputfilename = NULL;

    int n_switch = 0;
    size_t user_bytes_to_change = 1;

    int o_switch = 0;
    char *outputfilename = NULL;

    int s_switch = 0;
    size_t user_start_offset = 0;

    int u_switch = 0;

    int c;
    opterr = 0;

    while((c = getopt(argc, argv, "b:e:f:n:o:s:u")) != -1){
        switch(c){
            case 'b':
                {
                    b_switch = 1;
                    char *bnptr = NULL;
                    user_fuzz_byte = strtoul(optarg, &bnptr, 0);

                    if(*bnptr){
                        printf("b switch: invalid value '%s'\n", optarg);
                        return 1;
                    }

                    break;
                }
            case 'e':
                {
                    e_switch = 1;
                    char *enptr = NULL;
                    user_end_offset = strtoull(optarg, &enptr, 0);

                    if(*enptr){
                        printf("e switch: invalid value '%s'\n", optarg);
                        return 1;
                    }

                    break;
                }
            case 'f':
                {
                    inputfilename = strdup(optarg);
                    break;
                }
            case 'n':
                {
                    n_switch = 1;
                    char *nnptr = NULL;
                    user_bytes_to_change = strtoull(optarg, &nnptr, 0);

                    if(*nnptr){
                        printf("n switch: invalid value '%s'\n", optarg);
                        return 1;
                    }

                    break;
                }
            case 'o':
                {
                    outputfilename = strdup(optarg);
                    break;
                }
            case 's':
                {
                    s_switch = 1;
                    char *snptr = NULL;
                    user_start_offset = strtoull(optarg, &snptr, 0);

                    if(*snptr){
                        printf("s switch: invalid value '%s'\n", optarg);
                        return 1;
                    }

                    break;
                }
            case 'u':
                {
                    u_switch = 1;
                    break;
                }
            default:
                {
                    break;
                }
        };
    }

    int bkpthere = 0;

    if(s_switch && e_switch && user_start_offset >= user_end_offset){
        printf("Your start offset, %#zx, is >= to your end offset, %#zx\n",
                user_start_offset, user_end_offset);

        return 1;
    }

    if(!inputfilename){
        printf("No input file name\n");

        free(outputfilename);

        return 1;
    }

    char *extension = NULL;
    char *dot = strrchr(inputfilename, '.');

    if(!dot || !*(dot + 1)){
        printf("No extension on input file '%s'\n", inputfilename);
        
        free(inputfilename);
        free(outputfilename);

        return 1;
    }

    if(!outputfilename){
        time_t rawtime = time(NULL);
        struct tm *ptm = localtime(&rawtime);

        char timestr[0x100] = {0};
        strftime(timestr, sizeof(timestr), "%c", ptm);

        concat(&outputfilename, "%s-fuzzed-%s.%s", inputfilename, timestr, dot + 1);
    }

    printf("input file '%s' output file '%s'\n", inputfilename, outputfilename);

    printf("b switch: %d\n", b_switch);
    if(b_switch)
        printf("user_fuzz_byte: %c\n", user_fuzz_byte);

    printf("e switch: %d\n", e_switch);
    if(e_switch)
        printf("user_end_offset: %#zx\n", user_end_offset);

    printf("n switch: %d\n", n_switch);
    if(n_switch)
        printf("user_bytes_to_change: %#zx\n", user_bytes_to_change);

    printf("s switch: %d\n", s_switch);
    if(s_switch)
        printf("user_start_offset: %#zx\n", user_start_offset);

    printf("u switch: %d\n", u_switch);

    struct stat st;

    if(stat(inputfilename, &st)){
        printf("stat: input file '%s': %s\n", inputfilename, strerror(errno));

        free(inputfilename);
        free(outputfilename);

        return 1;
    }

    int inputfilefd = open(inputfilename, O_RDONLY, 0);

    if(inputfilefd == -1){
        printf("open: input file '%s': %s\n", inputfilename, strerror(errno));

        free(inputfilename);
        free(outputfilename);

        return 1;
    }

    size_t inputfilesize = st.st_size;

    if(!e_switch)
        user_end_offset = inputfilesize;

    if(s_switch && user_start_offset > inputfilesize){
        printf("Your start offset, %#zx, is larger than input file size, %#zx\n",
                user_start_offset, inputfilesize);

        free(inputfilename);
        free(outputfilename);

        close(inputfilefd);

        return 1;
    }

    if(e_switch && user_end_offset > inputfilesize){
        printf("Your end offset, %#zx, is larger than input file size, %#zx\n",
                user_end_offset, inputfilesize);

        free(inputfilename);
        free(outputfilename);

        close(inputfilefd);

        return 1;
    }

    /* modifications to this memory will not be reflected in the input file */
    void *inputfiledata = mmap(NULL, inputfilesize, PROT_READ | PROT_WRITE,
            MAP_PRIVATE, inputfilefd, 0);

    close(inputfilefd);

    if(inputfiledata == (void *)-1){
        printf("mmap: input file '%s': %s\n", inputfilename, strerror(errno));

        free(inputfilename);
        free(outputfilename);

        return 1;
    }

    /* *(char*)inputfiledata = 0x41; */

    printf("ok\n");

    /* we're allowed to fuzz fuzzsz bytes from user_start_offset */
    size_t fuzzsz = user_end_offset - user_start_offset;

    printf("allowed to fuzz %#zx bytes starting from offset %#zx\n", fuzzsz,
            user_start_offset);
    // XXX if unique, be sure to check if we've changed all avaailable bytes



    munmap(inputfiledata, inputfilesize);


    return 0;
}
