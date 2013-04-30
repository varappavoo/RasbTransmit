// To run:
//  wget -O - http://beta.etherpad.org/p/pihackfm/export/txt 2>/dev/null | gcc -lm -std=c99 -g -xc - && ./a.out beatles.wav
// Access from ARM Running Linux


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define DEFAULT_PORT (5234)

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
char *gpio_mem, *gpio_map;
char *spi0_mem, *spi0_map;


// I/O access
volatile unsigned *gpio;
volatile unsigned *allof7e;

// Two socket descriptors which are just integer numbers used to access a socket
int sock_descriptor, conn_desc;

// Two socket address structures - One for the server itself and the other for client
struct sockaddr_in serv_addr, client_addr;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(gpio+13)  // sets   bits which are 1 ignores bits which are 0

#define ACCESS(base) *(volatile int*)((int)allof7e+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

#define CM_GP0CTL (0x7e101070)
#define GPFSEL0 (0x7E200000)
#define CM_GP0DIV (0x7e101074)
#define CLKBASE (0x7E101000)
#define DMABASE (0x7E007000)
#define PWMBASE  (0x7e20C000) /* PWM controller */

#define TIMEOUT_TCP (1000)

struct GPCTL
{
    char SRC         : 4;
    char ENAB        : 1;
    char KILL        : 1;
    char             : 1;
    char BUSY        : 1;
    char FLIP        : 1;
    char MASH        : 2;
    unsigned int     : 13;
    char PASSWD      : 8;
};



void getRealMemPage(void** vAddr, void** pAddr)
{
    void* a = valloc(4096);

    ((int*)a)[0] = 1;  // use page to force allocation.

    mlock(a, 4096);  // lock into ram

    *vAddr = a;  // yay - we know the virtual address

    unsigned long long frameinfo;

    int fp = open("/proc/self/pagemap", 'r');
    lseek(fp, ((int)a)/4096*8, SEEK_SET);
    read(fp, &frameinfo, sizeof(frameinfo));

    *pAddr = (void*)((int)(frameinfo*4096));
}

void freeRealMemPage(void* vAddr)
{

    munlock(vAddr, 4096);  // unlock ram.

    free(vAddr);
}

int readall(int fd, void *buf, size_t nbytes)
{
    size_t done = 0;
    size_t now;
    int misses = 0;

    while ((done += (now = read(fd, buf, nbytes - done))) < nbytes) {
        buf += now;

        if (now < 0) {
            fprintf(stderr, "Receiving error!\n");
            return 0;
        } else if (now == 0) {
            misses++;
            if (misses > TIMEOUT_TCP)
                return 0;
        } else if (misses != 0)
            misses = 0;
    }
    return 1;
}

void readIntArray(int fd, uint32_t *buf, int size) {
    uint32_t checksum = 123;
    uint32_t tocheck;
    register int i;

    for (i = 0; i < size; i++) {
        readall(fd, &tocheck, sizeof(uint32_t));
        buf[i] = ntohl(tocheck);
        checksum ^= buf[i];
    }

    readall(fd, &tocheck, sizeof(uint32_t));
    tocheck = ntohl(tocheck);

    if (checksum != tocheck)
    {
        fprintf(stderr, "Checksum mismatch!");
        exit(1);
    }
}

void setup_fm()
{

    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0)
    {
        printf("can't open /dev/mem Reeason: %s\n", strerror(errno));
        exit (-1);
    }

    allof7e = (unsigned *)mmap(
                  NULL,
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x20000000  //base
              );

    if ((int)allof7e==-1) exit(-1);

    SETBIT(GPFSEL0 , 14);
    CLRBIT(GPFSEL0 , 13);
    CLRBIT(GPFSEL0 , 12);


    struct GPCTL setupword = {6/*SRC*/, 1, 0, 0, 0, 1,0x5a};

    ACCESS(CM_GP0CTL) = *((int*)&setupword);
}


//void modulate(int m)
//{
//    ACCESS(CM_GP0DIV) = (0x5a << 24) + 0x4d72 + m;
//}

struct CB
{
    volatile unsigned int TI;
    volatile unsigned int SOURCE_AD;
    volatile unsigned int DEST_AD;
    volatile unsigned int TXFR_LEN;
    volatile unsigned int STRIDE;
    volatile unsigned int NEXTCONBK;
    volatile unsigned int RES1;
    volatile unsigned int RES2;

};

struct DMAregs
{
    volatile unsigned int CS;
    volatile unsigned int CONBLK_AD;
    volatile unsigned int TI;
    volatile unsigned int SOURCE_AD;
    volatile unsigned int DEST_AD;
    volatile unsigned int TXFR_LEN;
    volatile unsigned int STRIDE;
    volatile unsigned int NEXTCONBK;
    volatile unsigned int DEBUG;
};

struct PageInfo
{
    void* p;  // physical address
    void* v;   // virtual address
};

struct PageInfo constPage;
struct PageInfo instrPage;
struct PageInfo instrs[1024];


void play(float samplerate)
{
    int fp= STDIN_FILENO;

    short data;
    const int clocksPerSample = round(22500.0*1390.0/(float)samplerate);  // for timing
    const int clocksPerSample2 = clocksPerSample/2;
    printf("Clocks per sample %d, half clocks %d\n", clocksPerSample, clocksPerSample2);
    const int cPp1 = (int)constPage.p + 2048 + 4;
    const int cPp2 = (int)constPage.p + 2048 - 4;
    register struct PageInfo * pi = instrs;
    const struct PageInfo * pi_max = instrs + 1024;
    register volatile int* status = (volatile int*)((int)allof7e+DMABASE + 0x04-0x7e000000);

    while (readall(conn_desc, &data, 2))
    {

        const int bigval = data << 5;
        const int intval4 = (bigval >> 15) << 2;
        const unsigned int fracval = (((bigval- (intval4 << 13))*clocksPerSample) >> 16) + clocksPerSample2;

        ++pi;
        while( *status ==  (int)(pi->p)) usleep(1000);
        ((struct CB*)(pi->v))->SOURCE_AD = cPp2 + intval4;

        ++pi;
        while( *status ==  (int)(pi->p)) usleep(1000);
        ((struct CB*)(pi->v))->TXFR_LEN = clocksPerSample-fracval;

        ++pi;
        while( *status ==  (int)(pi->p)) usleep(1000);
        ((struct CB*)(pi->v))->SOURCE_AD = cPp1 + intval4;

        ++pi;
        if (pi >= pi_max) pi = instrs;
        while( *status ==  (int)(pi->p)) usleep(1000);
        ((struct CB*)(pi->v))->TXFR_LEN = fracval;


    }
    close(fp);
}

void unSetupDMA()
{
    printf("exiting\n");
    struct DMAregs* DMA0 = (struct DMAregs*)&(ACCESS(DMABASE));
    DMA0->CS =1<<31;  // reset dma controller

    // close connections
    close(conn_desc);
    close(sock_descriptor);
}

void handSig()
{
    exit(0);
}
void setupDMA( float centerFreq )
{
    register int i;



    // allocate a few pages of ram
    getRealMemPage(&constPage.v, &constPage.p);

    int centerFreqDivider = (int)((500.0 / centerFreq) * (float)(1<<12) + 0.5);

    // make data page contents - it's essientially 1024 different commands for the
    // DMA controller to send to the clock module at the correct time.
    for (i=0; i<1024; i++)
        ((int*)(constPage.v))[i] = (0x5a << 24) + centerFreqDivider - 512 + i;


    int instrCnt = 0;

    while (instrCnt<1024)
    {
        getRealMemPage(&instrPage.v, &instrPage.p);

        // make copy instructions
        struct CB* instr0= (struct CB*)instrPage.v;

        for (i=0; i<4096/sizeof(struct CB); i++)
        {
            instrs[instrCnt].v = (void*)((int)instrPage.v + sizeof(struct CB)*i);
            instrs[instrCnt].p = (void*)((int)instrPage.p + sizeof(struct CB)*i);
            instr0->SOURCE_AD = (unsigned int)constPage.p+2048;
            instr0->DEST_AD = PWMBASE+0x18 /* FIF1 */;
            instr0->TXFR_LEN = 4;
            instr0->STRIDE = 0;
            //instr0->NEXTCONBK = (int)instrPage.p + sizeof(struct CB)*(i+1);
            instr0->TI = (1/* DREQ  */<<6) | (5 /* PWM */<<16) |  (1<<26/* no wide*/) ;
            instr0->RES1 = 0;
            instr0->RES2 = 0;

            if (i%2)
            {
                instr0->DEST_AD = CM_GP0DIV;
                instr0->STRIDE = 4;
                instr0->TI = (1<<26/* no wide*/) ;
            }

            if (instrCnt!=0) ((struct CB*)(instrs[instrCnt-1].v))->NEXTCONBK = (int)instrs[instrCnt].p;
            instr0++;
            instrCnt++;
        }
    }
    ((struct CB*)(instrs[1023].v))->NEXTCONBK = (int)instrs[0].p;

    // set up a clock for the PWM
    ACCESS(CLKBASE + 40*4 /*PWMCLK_CNTL*/) = 0x5A000026;
    usleep(1000);
    ACCESS(CLKBASE + 41*4 /*PWMCLK_DIV*/)  = 0x5A002800;
    ACCESS(CLKBASE + 40*4 /*PWMCLK_CNTL*/) = 0x5A000016;
    usleep(1000);

    // set up pwm
    ACCESS(PWMBASE + 0x0 /* CTRL*/) = 0;
    usleep(1000);
    ACCESS(PWMBASE + 0x4 /* status*/) = -1;  // clear errors
    usleep(1000);
    ACCESS(PWMBASE + 0x0 /* CTRL*/) = -1; //(1<<13 /* Use fifo */) | (1<<10 /* repeat */) | (1<<9 /* serializer */) | (1<<8 /* enable ch */) ;
    usleep(1000);
    ACCESS(PWMBASE + 0x8 /* DMAC*/) = (1<<31 /* DMA enable */) | 0x0707;

    //activate dma
    struct DMAregs* DMA0 = (struct DMAregs*)&(ACCESS(DMABASE));
    DMA0->CS =1<<31;  // reset
    DMA0->CONBLK_AD=0;
    DMA0->TI=0;
    DMA0->CONBLK_AD = (unsigned int)(instrPage.p);
    DMA0->CS =(1<<0)|(255 <<16);  // enable bit = 0, clear end flag = 1, prio=19-16
}



void startserver(int port, float * samprate, float * freq)
{

    // Create socket of domain - Internet (IP) address, type - Stream based (TCP) and protocol unspecified
    // since it is only useful when underlying stack allows more than one protocol and we are choosing one.
    // 0 means choose the default protocol.
    sock_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    // A valid descriptor is always a positive value
    if(sock_descriptor < 0)
        printf("Failed creating socket\n");

    // Initialize the server address struct to zero
    bzero((char *)&serv_addr, sizeof(serv_addr));

    // Fill server's address family
    serv_addr.sin_family = AF_INET;

    // Server should allow connections from any ip address
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // 16 bit port number on which server listens
    // The function htons (host to network short) ensures that an integer is interpretted
    // correctly (whether little endian or big endian) even if client and server have different architectures
    serv_addr.sin_port = htons(port);

    // Attach the server socket to a port. This is required only for server since we enforce
    // that it does not select a port randomly on it's own, rather it uses the port specified
    // in serv_addr struct.
    if (bind(sock_descriptor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        printf("Failed to bind\n");

    // Server should start listening - This enables the program to halt on accept call (coming next)
    // and wait until a client connects. Also it specifies the size of pending connection requests queue
    // i.e. in this case it is 5 which means 5 clients connection requests will be held pending while
    // the server is already processing another connection request.
    listen(sock_descriptor, 5);

    printf("Waiting for connection on port %d...\n", port);
    socklen_t size = sizeof(client_addr);

    // Server blocks on this call until a client tries to establish connection.
    // When a connection is established, it returns a 'connected socket descriptor' different
    // from the one created earlier.
    conn_desc = accept(sock_descriptor, (struct sockaddr *)&client_addr, &size);
    if (conn_desc == -1)
        printf("Failed accepting connection\n");
    else
        printf("Connected\n");

    // The new descriptor can be simply read from / written up just like a normal file descriptor
//    unsigned char blah;
//    while ( read(conn_desc, &blah, sizeof(blah)) > 0 ) {
//        printf("%x ", blah);
//        fflush(stdout);
//    }
//    printf("\n ", blah);

#define HEADER_SIZE (2)
    uint32_t header[HEADER_SIZE];
    readIntArray(conn_desc, header, HEADER_SIZE);

    *samprate   = header[0];
    *freq       = header[1] / 1000000.0f;
}

int main(int argc, char **argv)
{
    atexit(unSetupDMA);
    signal (SIGINT, handSig);
    signal (SIGTERM, handSig);
    signal (SIGHUP, handSig);
    signal (SIGQUIT, handSig);

    float samp;
    float freq;

    while (1) {
        setup_fm();
        startserver(argc>2 ? atof(argv[2]):DEFAULT_PORT, &samp, &freq);
        setupDMA(freq);
        play(samp);
        unSetupDMA();
    }

    return 0;

} // main

