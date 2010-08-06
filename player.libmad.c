#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <mad.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <err.h>

int audio_init(int rate, int channels)
{
    int fd;
    const int fmt = AFMT_S16_NE;
    static int once = 0;

    if (rate != 44100 || channels != 2)
        fprintf(stderr, "%d:%d\n", rate, channels);

    if (once != 0)
        return 0;
    once = 1;

    if ((fd = open("/dev/audio", O_WRONLY)) < 0)
        return -1;

    if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) < 0 ||
        ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0 ||
        dup2(fd, 1) < 0 ||
        close(fd) < 0) {

        close(fd);
        return -1;
    }

    return 0;
}

enum mad_flow input(void *x, struct mad_stream *stream)
{
#if 1
    int fd;
    char buf[4457665];

    if ((fd = open("z.mp3", O_RDONLY)) < 0)
        err(1, "open");
    if (read(fd, buf, sizeof(buf)) != sizeof(buf))
        err(1, "read");
    close(fd);

    mad_stream_buffer(stream, buf, sizeof(buf));
#else
    ssize_t n;
    unsigned char data[4096];

    if ((n = read(0, data, sizeof(data))) <= 0)
        return MAD_FLOW_STOP;

    mad_stream_buffer(stream, data, n);
#endif

    return MAD_FLOW_CONTINUE;
}

enum mad_flow header(void *data, struct mad_header const *header)
{
    int channels;

    channels = 2;
    if (header->mode == MAD_MODE_SINGLE_CHANNEL)
        channels = 1;

    if (audio_init(header->samplerate, channels) < 0)
        return MAD_FLOW_STOP;

    return MAD_FLOW_CONTINUE;
}

int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

enum mad_flow output(void *x,
                     struct mad_header const *stream,
                     struct mad_pcm *pcm)
{
    signed short __attribute__ ((aligned (16))) samples[1152*2];

#if 0
#define NL "\n\t"
    int i, len;
    const mad_fixed_t *L, *R;
    signed short *o;

    // xmm14: ROUND: { 0x1000, 0x1000, 0x1000, 0x1000 }
    // xmm15: `0'
    asm("pcmpeqd %xmm0,%xmm0" NL
        "pxor %xmm14,%xmm14" NL
        "pxor %xmm15,%xmm15" NL
        "psubd %xmm0,%xmm14" NL
        "pslld $12,%xmm14");

    L = pcm->samples[0];
    R = pcm->samples[1];
    o = samples;
    len = pcm->length;
    for (i = 0; i < len; i += 8) {
        asm("movdqu %0,%%xmm0":: "m"(L[i]));
        asm("movdqu %0,%%xmm1":: "m"(R[i]));
        asm("movdqu %0,%%xmm2":: "m"(L[i+4]));
        asm("movdqu %0,%%xmm3":: "m"(R[i+4]));

        asm("paddd %xmm14,%xmm0" NL
            "paddd %xmm14,%xmm1" NL
            "paddd %xmm14,%xmm2" NL
            "paddd %xmm14,%xmm3");

        asm("psrad $13,%xmm0" NL
            "psrad $13,%xmm1" NL
            "psrad $13,%xmm2" NL
            "psrad $13,%xmm3");

        asm("packssdw %xmm2,%xmm0" NL
            "packssdw %xmm3,%xmm1" NL
            "movdqa %xmm0,%xmm2" NL
            "punpcklwd %xmm1,%xmm0" NL
            "punpckhwd %xmm1,%xmm2");

        asm("movdqa %%xmm0,%0" NL
            "movdqa %%xmm2,%1": "=m"(o[i*2]), "=m"(o[i*2+8]));
    }
    //asm("emms");

#else
    int i;

    for (i = 0; i < pcm->length; i++) {
        samples[i*2] = scale(pcm->samples[0][i]);
        samples[i*2+1] = scale(pcm->samples[1][i]);
    }
#endif

    if (write(1, samples, sizeof(samples)) != sizeof(samples))
        return MAD_FLOW_STOP;

    return MAD_FLOW_CONTINUE;
}

enum mad_flow error(void *data,
                    struct mad_stream *stream,
                    struct mad_frame *frame)
{
    printf("error\n");
    printf("%s\n", mad_stream_errorstr(stream));
    return MAD_FLOW_CONTINUE;
}

enum mad_flow message(void *data, void *data2, unsigned int *i)
{
    printf("message");
    return MAD_FLOW_CONTINUE;
}

int main()
{
    struct mad_decoder dec;

    mad_decoder_init(&dec, NULL,
                     input,
                     header, //header,
                     0, //filter,
                     output,
                     error,
                     message);

    mad_decoder_run(&dec, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&dec);

    return 0;
}
