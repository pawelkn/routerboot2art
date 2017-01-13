#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

/*
 * Magic numbers
 */
#define RB_MAGIC_ERD            0x00455244

#define RB_MAGIC_HARD           0x64726148 /* "Hard" */
#define RB_MAGIC_SOFT           0x74666F53 /* "Soft" */
#define RB_MAGIC_DAWN           0x6E776144 /* "Dawn" */

#define RB_ID_TERMINATOR        0
#define RB_ID_WLAN_DATA         22

/*
 * Data types
 */
#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

/*
 * Memory offsets
 */
#define RB_ROUTERBOOT_OFFSET    0x0000
#define RB_ROUTERBOOT_SIZE      0xe000
#define RB_HARD_CFG_OFFSET      0xe000
#define RB_HARD_CFG_SIZE        0x1000

#define RB_SIZE                 0x20000
#define ART_SIZE                0x10000

static u16 get_u16(void *buf)
{
    u8 *p = (u8*)buf;
    return (u16) p[1] + ((u16) p[0] << 8);
}

static u32 get_u32(void *buf)
{
    u8 *p = (u8*)buf;
    return ((u32) p[3] + ((u32) p[2] << 8) + ((u32) p[1] << 16) +
           ((u32) p[0] << 24));
}

int routerboot_find_magic(u8 *buf, unsigned int buflen, u32 *offset, bool hard)
{
    u32 magic_ref = hard ? RB_MAGIC_HARD : RB_MAGIC_SOFT;
    u32 magic;
    u32 cur = *offset;

    while ( cur < buflen ) {
        magic = get_u32( buf + cur );
        if ( magic == magic_ref ) {
            *offset = cur;
            return 0;
        }

        cur += 0x1000;
    }

    return -1;
}

int routerboot_find_tag(u8 *buf, unsigned int buflen, u16 tag_id,
            u8 **tag_data, u16 *tag_len)
{
    u32 magic;
    bool align = false;
    int ret;

    if (buflen < 4)
        return -1;

    magic = get_u32(buf);
    switch (magic) {
    case RB_MAGIC_ERD:
        align = true;
        /* fall trough */
    case RB_MAGIC_HARD:
        /* skip magic value */
        buf += 4;
        buflen -= 4;
        break;

    case RB_MAGIC_SOFT:
        if (buflen < 8)
            return -1;

        /* skip magic and CRC value */
        buf += 8;
        buflen -= 8;

        break;

    default:
        return -1;
    }

    ret = -1;
    while (buflen > 2) {
        u16 id;
        u16 len;

        len = get_u16(buf);
        buf += 2;
        buflen -= 2;

        if (buflen < 2)
            break;

        id = get_u16(buf);
        buf += 2;
        buflen -= 2;

        if (id == RB_ID_TERMINATOR)
            break;

        if (buflen < len)
            break;

        if (id == tag_id) {
            if (tag_len)
                *tag_len = len;
            if (tag_data)
                *tag_data = buf;
            ret = 0;
            break;
        }

        if (align)
            len = (len + 3) / 4;

        buf += len;
        buflen -= len;
    }

    return ret;
}

u32 rle_decode(const u8 *src, u32 srclen,
           u8 *dst, u32 dstlen,
           u32 *src_done, u32 *dst_done)
{
    u32 srcpos, dstpos;
    int ret;

    srcpos = 0;
    dstpos = 0;
    ret = -1;

    /* sanity checks */
    if (!src || !srclen || !dst || !dstlen)
        goto out;

    while (1) {
        char count;

        if (srcpos >= srclen)
            break;

        count = (char) src[srcpos++];
        if (count == 0) {
            ret = 0;
            break;
        }

        if (count > 0) {
            u8 c;

            if (srcpos >= srclen)
                break;

            c = src[srcpos++];

            while (count--) {
                if (dstpos >= dstlen)
                    break;

                dst[dstpos++] = c;
            }
        } else {
            count *= -1;

            while (count--) {
                if (srcpos >= srclen)
                    break;
                if (dstpos >= dstlen)
                    break;
                dst[dstpos++] = src[srcpos++];
            }
        }
    }

out:
    if (src_done)
        *src_done = srcpos;
    if (dst_done)
        *dst_done = dstpos;

    return ret;
}

void read_file( const char* filename, u8* buf, u32 buf_len, u32* read )
{
    ifstream ifs( filename, ios::binary | ios::ate );
    u32 length = ifs.tellg();
    if( length > buf_len ) {
        length = buf_len;
    }

    ifs.seekg( 0, ios::beg );
    ifs.read( (char*)buf, length );
    *read = length;
}

void write_file( const char* filename, u8* buf, u32 buf_len )
{
    ofstream ifs( filename, ios::binary | ios::ate );
    ifs.write( (char*)buf, buf_len );
}

int main( int argc, char *argv[] )
{
    if( argc < 3 ) {
        cout << "Usage: routerboot2art [routerboot.bin] [art.bin]" << endl;
        return -1;
    }

    u8 input[RB_SIZE];
    u32 input_len = 0;
    read_file( argv[1], input, RB_SIZE, &input_len );

    u32 hard_cfg_offset = 0;
    u32 err = routerboot_find_magic( input, input_len, &hard_cfg_offset, RB_MAGIC_HARD );
    if( err ) {
        cerr << "Unable to find magic tag in input file." << endl;
        return -1;
    }

    u8* tag;
    u16 tag_len = 0;
    err = routerboot_find_tag( input + hard_cfg_offset, RB_HARD_CFG_SIZE, RB_ID_WLAN_DATA,
                               &tag, &tag_len );
    if( err ) {
        cerr << "No calibration data found" << endl;
        return -1;
    }

    u8 output[ART_SIZE];
    err = rle_decode( tag, tag_len, output, ART_SIZE, NULL, NULL );
    if( err ) {
        cerr << "Unable to decode calibration data" << endl;
        return -1;
    }

    write_file( argv[2], output, ART_SIZE );
    return 0;
}
