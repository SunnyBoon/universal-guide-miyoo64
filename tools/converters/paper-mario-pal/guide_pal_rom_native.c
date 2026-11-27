/* Isolated native PAL Power Jump BMP prototype. */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>

typedef struct { uint32_t h[5]; uint64_t size; unsigned char block[64]; size_t used; } sha1_ctx;
static uint32_t rol(uint32_t x,unsigned n){return (x<<n)|(x>>(32-n));}
static void sha_block(sha1_ctx *c,const unsigned char *b){
 uint32_t w[80],a,d,e,f,k,t,bb,cc;unsigned i;
 for(i=0;i<16;i++)w[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|b[i*4+3];
 for(;i<80;i++)w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
 a=c->h[0];bb=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];
 for(i=0;i<80;i++){
  if(i<20){f=(bb&cc)|(~bb&d);k=0x5a827999;}else if(i<40){f=bb^cc^d;k=0x6ed9eba1;}else if(i<60){f=(bb&cc)|(bb&d)|(cc&d);k=0x8f1bbcdc;}else{f=bb^cc^d;k=0xca62c1d6;}
  t=rol(a,5)+f+e+k+w[i];e=d;d=cc;cc=rol(bb,30);bb=a;a=t;
 }
 c->h[0]+=a;c->h[1]+=bb;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;
}
static void sha_init(sha1_ctx *c){memset(c,0,sizeof(*c));c->h[0]=0x67452301;c->h[1]=0xefcdab89;c->h[2]=0x98badcfe;c->h[3]=0x10325476;c->h[4]=0xc3d2e1f0;}
static void sha_add(sha1_ctx *c,const unsigned char *p,size_t n){c->size+=n;while(n){size_t t=64-c->used;if(t>n)t=n;memcpy(c->block+c->used,p,t);c->used+=t;p+=t;n-=t;if(c->used==64){sha_block(c,c->block);c->used=0;}}}
static void sha_final(sha1_ctx *c,unsigned char out[20]){unsigned i;uint64_t bits=c->size*8;unsigned char one=0x80,zero=0,end[8];sha_add(c,&one,1);while(c->used!=56)sha_add(c,&zero,1);for(i=0;i<8;i++)end[7-i]=(unsigned char)(bits>>(i*8));sha_add(c,end,8);for(i=0;i<5;i++){out[i*4]=c->h[i]>>24;out[i*4+1]=c->h[i]>>16;out[i*4+2]=c->h[i]>>8;out[i*4+3]=c->h[i];}}
static int verify(FILE *f){
 static const unsigned char expected[20]={0x21,0x11,0xd3,0x92,0x65,0xa3,0x17,0x41,0x4d,0x35,0x9e,0x35,0xa7,0xd9,0x71,0xc4,0xdf,0xa5,0xf9,0xe1};
 unsigned char b[65536],header[64],digest[20];size_t n;struct stat st;sha1_ctx sha;
 if(fstat(fileno(f),&st)||st.st_size!=67108864){fputs("Falsche ROM-Größe.\n",stderr);return 4;}
 if(fread(header,1,sizeof(header),f)!=sizeof(header)){fputs("ROM nicht lesbar.\n",stderr);return 3;}
 if(memcmp(header,"\x80\x37\x12\x40",4)){fputs("Ungültiges ROM-Format.\n",stderr);return 8;}
 if(memcmp(header+0x20,"PAPER MARIO",11)||memcmp(header+0x3b,"NMQP",4)){fputs("Falsche ROM-Region.\n",stderr);return 9;}
 if(header[0x3f]){fputs("Falsche ROM-Revision.\n",stderr);return 10;}
 if(fseek(f,0,SEEK_SET)){return 3;}sha_init(&sha);while((n=fread(b,1,sizeof(b),f))!=0)sha_add(&sha,b,n);
 if(ferror(f)){return 3;}sha_final(&sha,digest);
 if(memcmp(digest,expected,20)){fputs("Falsche oder beschädigte PAL-ROM.\n",stderr);return 5;}
 return 0;
}

static void le16(unsigned char *p, unsigned value) { p[0]=(unsigned char)value; p[1]=(unsigned char)(value>>8); }
static void le32(unsigned char *p, uint32_t value) { le16(p,value); le16(p+2,value>>16); }

static void render_ci4(const unsigned char raster[512],const unsigned char palette[32],unsigned char bmp[27702], int zero_dpi) {
    int x,y; size_t offset=54;
    memset(bmp,0,27702); bmp[0]='B'; bmp[1]='M';
    le32(bmp+2,27702); le32(bmp+10,54); le32(bmp+14,40);
    le32(bmp+18,96); le32(bmp+22,96); le16(bmp+26,1); le16(bmp+28,24);
    le32(bmp+34,96*96*3); if (!zero_dpi) { le32(bmp+38,2835); le32(bmp+42,2835); }
    for(y=95;y>=0;y--) for(x=0;x<96;x++) {
        int idx=(y/3)*32+x/3; unsigned packed=raster[idx/2];
        unsigned color_idx=(idx&1)?packed&15:packed>>4;
        unsigned color=((unsigned)palette[color_idx*2]<<8)|palette[color_idx*2+1];
        unsigned r=20,g=27,b=38;
        if(color&1) { r=(((color>>11)&31)*255+30)/31; g=(((color>>6)&31)*255+30)/31; b=(((color>>1)&31)*255+30)/31; }
        bmp[offset++]=(unsigned char)b; bmp[offset++]=(unsigned char)g; bmp[offset++]=(unsigned char)r;
    }
}

/* Raster and palette offsets are the PowerJump entry in guide_pal_badge_recipe.json. */
static int powerjump(FILE *rom, const char *directory) {
    unsigned char raster[512], palette[32], bmp[54+96*96*3];
    char path[PATH_MAX], resolved[PATH_MAX];
    int fd; size_t written=0; ssize_t n;
    if (!realpath(directory,resolved) || strncmp(resolved,"/tmp/",5)!=0 || !strcmp(resolved,"/tmp/")) {
        fputs("Ziel muss ein bestehendes Testverzeichnis unter /tmp sein.\n",stderr); return 6;
    }
    if (snprintf(path,sizeof(path),"%s/PowerJump.bmp",resolved)>=(int)sizeof(path)) return 6;
    if (fseek(rom,2055072L,SEEK_SET) || fread(raster,1,sizeof(raster),rom)!=sizeof(raster) ||
        fseek(rom,2055584L,SEEK_SET) || fread(palette,1,sizeof(palette),rom)!=sizeof(palette)) {
        fputs("ROM-Ressourcen nicht lesbar.\n",stderr); return 3;
    }
    render_ci4(raster,palette,bmp,0);
    fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0) { fprintf(stderr,"Testbild nicht erstellt: %s\n",strerror(errno)); return 6; }
    while(written<sizeof(bmp)) {
        n=write(fd,bmp+written,sizeof(bmp)-written);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) { int saved=errno; close(fd); unlink(path); errno=saved; fputs("Fehler beim Schreiben.\n",stderr); return 6; }
        written+=(size_t)n;
    }
    if(close(fd)) { unlink(path); fputs("Fehler beim Schließen.\n",stderr); return 6; }
    printf("Erzeugt: %s\n",path); return 0;
}
typedef struct {
    char id[128],method[128],output[128],hash[65];
    long raster,palette,vertex,sprite,sprite_palette,display[6]; int width,height,needed,existed; struct stat before;
} asset;
static unsigned rd16(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static uint32_t rd32(const unsigned char *p){return rd16(p)|((uint32_t)rd16(p+2)<<16);}
static int is_valid_bmp(const unsigned char *p,size_t n) {
    uint32_t off,w,h,bits;
    if(n<54 || n>1048576 || p[0]!='B' || p[1]!='M' || rd32(p+14)!=40 || rd16(p+26)!=1 || rd32(p+30)!=0) return 0;
    off=rd32(p+10);w=rd32(p+18);h=rd32(p+22);bits=rd16(p+28);
    if(!w||w>512||!h||h>512||(bits!=24&&bits!=32)||off<54) return 0;
    return (uint64_t)off + (((uint64_t)w*bits+31)/32)*4*h<=n;
}
/* SHA-256 (FIPS 180-4), local implementation to avoid runtime crypto dependencies. */
static uint32_t ror32(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static void sha256_block(uint32_t h[8],const unsigned char b[64]){
 static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t w[64],a,b0,c,d,e,f,g,z,t1,t2;unsigned i;
 for(i=0;i<16;i++)w[i]=((uint32_t)b[4*i]<<24)|((uint32_t)b[4*i+1]<<16)|((uint32_t)b[4*i+2]<<8)|b[4*i+3];
 for(;i<64;i++)w[i]=w[i-16]+(ror32(w[i-15],7)^ror32(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(ror32(w[i-2],17)^ror32(w[i-2],19)^(w[i-2]>>10));
 a=h[0];b0=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];z=h[7];
 for(i=0;i<64;i++){t1=z+(ror32(e,6)^ror32(e,11)^ror32(e,25))+((e&f)^(~e&g))+k[i]+w[i];t2=(ror32(a,2)^ror32(a,13)^ror32(a,22))+((a&b0)^(a&c)^(b0&c));z=g;g=f;f=e;e=d+t1;d=c;c=b0;b0=a;a=t1+t2;}
 h[0]+=a;h[1]+=b0;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
static void sha256_digest(const unsigned char *p,size_t n,unsigned char out[32]){
 uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 unsigned char tail[128]={0};size_t blocks=n/64,remain=n%64,i,pad;uint64_t bits=(uint64_t)n*8;
 for(i=0;i<blocks;i++)sha256_block(h,p+i*64);
 memcpy(tail,p+blocks*64,remain);tail[remain]=0x80;pad=remain<56?64:128;
 for(i=0;i<8;i++){tail[pad-1-i]=(unsigned char)(bits>>(8*i));}
 sha256_block(h,tail);if(pad==128)sha256_block(h,tail+64);
 for(i=0;i<8;i++){out[i*4]=h[i]>>24;out[i*4+1]=h[i]>>16;out[i*4+2]=h[i]>>8;out[i*4+3]=h[i];}
}

static int hash_matches(const unsigned char *p,size_t n,const char *expected) {
    unsigned char digest[32];char hex[65];unsigned i;
    sha256_digest(p,n,digest);
    for(i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
    return strcmp(hex,expected)==0;
}
/* The existing JSON is the single source of offsets and SHA-256s. */
typedef struct { int x,y,z,u,v,r,g,b; } vertex;
static unsigned be16(const unsigned char *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t be32(const unsigned char *p){return ((uint32_t)be16(p)<<16)|be16(p+2);}
static int read_at(FILE *f,long off,unsigned char *buf,size_t n){return !fseek(f,off,SEEK_SET)&&fread(buf,1,n,f)==n;}
static void color_ci4(const unsigned char *tex,const unsigned char pal[32],int width,int x,int y,int rgba[4]){
 int ix=y*width+x,index=(ix&1)?tex[ix/2]&15:tex[ix/2]>>4;unsigned c=be16(pal+index*2);
 rgba[0]=(((c>>11)&31)*255+30)/31;rgba[1]=(((c>>6)&31)*255+30)/31;rgba[2]=(((c>>1)&31)*255+30)/31;rgba[3]=(c&1)?255:0;
}
/* Direct translation of guide_render_superblock.py's fixed orthographic view. */
static int render_superblock(FILE *f,const asset *a,unsigned char *bmp){
 unsigned char raw[1152],tex[256],pal[32],sprite[512],spal[32],cmd[8];
 vertex vs[72];int tris[40][3],nt=0,i,j,k,x,y,slots[32];
 unsigned char *rgb=NULL;double *depth=NULL;int ok=0;
 if(!read_at(f,a->vertex,raw,sizeof(raw))||!read_at(f,a->raster,tex,sizeof(tex))||!read_at(f,a->palette,pal,32)||!read_at(f,a->sprite,sprite,sizeof(sprite))||!read_at(f,a->sprite_palette,spal,32))return 0;
 for(i=0;i<72;i++){unsigned char *v=raw+i*16;vs[i]=(vertex){(int16_t)be16(v),(int16_t)be16(v+2),(int16_t)be16(v+4),(int16_t)be16(v+8),(int16_t)be16(v+10),v[12],v[13],v[14]};}
 for(i=0;i<6;i++){
  int end=0;for(j=0;j<32;j++)slots[j]=-1;if(fseek(f,a->display[i],SEEK_SET))return 0;
  for(j=0;j<40;j++){
   unsigned word,addr,count,last,first;if(fread(cmd,1,8,f)!=8)return 0;
   if(cmd[0]==0xdf){end=1;break;}
   if(cmd[0]==1){word=be32(cmd);addr=be32(cmd+4);count=(word>>12)&255;last=(word&255)/2;first=(addr&0xffffff)/16;
    if((addr>>24)!=10 || (addr&15) || last<count || last>32 || first+count>72)return 0;
    for(k=0;k<(int)count;k++)slots[last-count+k]=(int)first+k;
   }
   if(cmd[0]==6){int inds[6]={cmd[1],cmd[2],cmd[3],cmd[5],cmd[6],cmd[7]};if(nt+2>40)return 0;
    for(k=0;k<6;k++){int idx=inds[k]/2;if((inds[k]&1)||idx>=32||slots[idx]<0)return 0;tris[nt+k/3][k%3]=slots[idx];}nt+=2;}
  }
  if(!end)return 0;
 }
 if(nt!=40)return 0;
 rgb=malloc(200*130*3);depth=malloc(200*130*sizeof(*depth));if(!rgb||!depth)goto done;
 for(i=0;i<200*130;i++){rgb[3*i]=20;rgb[3*i+1]=27;rgb[3*i+2]=38;depth[i]=-1e9;}
 for(i=0;i<40;i++){
  vertex v[3];double px[3],py[3],pz[3],den,minx,maxx,miny,maxy;int xmin,xmax,ymin,ymax;
  for(k=0;k<3;k++){v[k]=vs[tris[i][k]];px[k]=100+(v[k].x-v[k].z)*2.05;py[k]=105-v[k].y*2.6+(v[k].x+v[k].z)*0.8;pz[k]=v[k].x+v[k].z+v[k].y*0.8;}
  den=(py[1]-py[2])*(px[0]-px[2])+(px[2]-px[1])*(py[0]-py[2]);if(fabs(den)<1e-6)continue;
  minx=fmin(px[0],fmin(px[1],px[2]));maxx=fmax(px[0],fmax(px[1],px[2]));miny=fmin(py[0],fmin(py[1],py[2]));maxy=fmax(py[0],fmax(py[1],py[2]));
  xmin=(int)minx;if(xmin<0)xmin=0;xmax=(int)maxx+1;if(xmax>199)xmax=199;
  ymin=(int)miny;if(ymin<0)ymin=0;ymax=(int)maxy+1;if(ymax>129)ymax=129;
  for(y=ymin;y<=ymax;y++)for(x=xmin;x<=xmax;x++){
   double q[3],z=0,u=0,tv=0,shade[3]={0,0,0};int ix,tx,ty,c[4],col[3];
   q[0]=((py[1]-py[2])*(x+0.5-px[2])+(px[2]-px[1])*(y+0.5-py[2]))/den;
   q[1]=((py[2]-py[0])*(x+0.5-px[2])+(px[0]-px[2])*(y+0.5-py[2]))/den;q[2]=1-q[0]-q[1];
   if(q[0]<-0.00001||q[1]<-0.00001||q[2]<-0.00001)continue;
   for(k=0;k<3;k++){z+=q[k]*pz[k];}
   ix=y*200+x;if(z<depth[ix])continue;
   for(k=0;k<3;k++){u+=q[k]*v[k].u;tv+=q[k]*v[k].v;shade[0]+=q[k]*v[k].r;shade[1]+=q[k]*v[k].g;shade[2]+=q[k]*v[k].b;}
   u/=32;tv/=32;tx=(int)floor(u)%32;if(tx<0)tx+=32;if(tx>=16)tx=31-tx;ty=(int)tv;if(ty<0)ty=0;if(ty>31)ty=31;
   color_ci4(tex,pal,16,tx,ty,c);for(k=0;k<3;k++)col[k]=(int)nearbyint(c[k]*shade[k]/255);
   for(k=0;k<3;k++){rgb[ix*3+k]=(unsigned char)col[k];}
   depth[ix]=z;
  }
 }
 /* 32x32 content sprite, as in the existing host renderer. */
 for(y=(int)(68.6-34);y<(int)(68.6+34);y++)for(x=66;x<134;x++){
  int sx=(int)((x-66)*32.0/68),sy=(int)((y-(68.6-34))*32/68),c[4],ix=y*200+x;
  color_ci4(sprite,spal,32,sx,sy,c);if(c[3] && 11.2>=depth[ix])for(k=0;k<3;k++)rgb[ix*3+k]=(unsigned char)((c[k]*c[3]+rgb[ix*3+k]*(255-c[3])+127)/255);
 }
 memset(bmp,0,78054);bmp[0]='B';bmp[1]='M';le32(bmp+2,78054);le32(bmp+10,54);le32(bmp+14,40);le32(bmp+18,200);le32(bmp+22,130);le16(bmp+26,1);le16(bmp+28,24);le32(bmp+34,78000);le32(bmp+38,2835);le32(bmp+42,2835);
 for(y=0;y<130;y++)for(x=0;x<200;x++){int src=((129-y)*200+x)*3,dst=54+(y*200+x)*3;bmp[dst]=rgb[src+2];bmp[dst+1]=rgb[src+1];bmp[dst+2]=rgb[src];}
 ok=1;
done:free(rgb);free(depth);return ok;
}

static int load_assets(const char *path,asset assets[72]) {
    FILE *f=fopen(path,"rb");char *json,*p;long size;int count=0;
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)||(size=ftell(f))<0||size>262144||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    json=malloc((size_t)size+1);if(!json){fclose(f);return 0;}
    if(fread(json,1,(size_t)size,f)!=(size_t)size){free(json);fclose(f);return 0;}fclose(f);json[size]=0;
    if(!strstr(json,"\"rom_size\": 67108864")||!strstr(json,"2111d39265a317414d359e35a7d971c4dfa5f9e1")){free(json);return 0;}
    p=json;
    while((p=strstr(p,"\"asset_id\""))!=NULL){
        asset a={0};int i,used=0;char *next;
        if(sscanf(p,"\"asset_id\": \"%127[^\"]\" , \"method\": \"%127[^\"]\" , \"raster_offset\": %ld , \"palette_offset\": %ld , \"width\": %d , \"height\": %d , \"output\": \"%127[^\"]\" , \"sha256\": \"%64[0-9a-f]\"%n",a.id,a.method,&a.raster,&a.palette,&a.width,&a.height,a.output,a.hash,&used)!=8){free(json);return 0;}
        next=p+used;p=next;
        if(!strcmp(a.method,"pal-superblock-orthographic-bmp24")) {
            int matched=sscanf(next," , \"vertex_offset\": %ld , \"sprite_offset\": %ld , \"sprite_palette_offset\": %ld , \"display_offsets\": [ %ld , %ld , %ld , %ld , %ld , %ld",&a.vertex,&a.sprite,&a.sprite_palette,&a.display[0],&a.display[1],&a.display[2],&a.display[3],&a.display[4],&a.display[5]);
            if(matched!=9 || a.width!=200 || a.height!=130 || strcmp(a.id,"entity/SuperBlock") || strcmp(a.output,"private/SuperBlock.bmp")){free(json);return 0;}
            if(a.vertex<0 || a.vertex>67108864-1152 || a.sprite<0 || a.sprite>67108864-512 || a.sprite_palette<0 || a.sprite_palette>67108864-32){free(json);return 0;}
            for(i=0;i<6;i++)if(a.display[i]<0 || a.display[i]>67108864-320){free(json);return 0;}
        } else {
        if(strcmp(a.method,"pal-ci4-rgba16-nearest3-bmp24")&&strcmp(a.method,"pal-ci4-rgba16-nearest3-bmp24-zero-dpi")){free(json);return 0;}
        if(a.width!=32 || a.height!=32 || a.raster<0 || a.raster>67108864-512 || a.palette<0 || a.palette>67108864-32 || strlen(a.hash)!=64){free(json);return 0;}
        }
        if(count>=72 || a.raster<0 || a.raster>67108864-512 || a.palette<0 || a.palette>67108864-32 || strlen(a.hash)!=64){free(json);return 0;}
        if(strcmp(a.output,"S1.bmp")) {
            const char *n=a.output;
            if(strncmp(n,"private/",8)){free(json);return 0;}n+=8;
            if(!*n){free(json);return 0;}
            for(;*n;n++)if(!isalnum((unsigned char)*n)&&*n!='_'&&*n!='-'&&*n!='.'){free(json);return 0;}
            if(strstr(a.output,"..") || strlen(a.output)<13 || strcmp(a.output+strlen(a.output)-4,".bmp")){free(json);return 0;}
        }
        for(i=0;i<count;i++)if(!strcmp(a.output,assets[i].output)||!strcmp(a.id,assets[i].id)){free(json);return 0;}
        assets[count++]=a;
    }
    free(json);return count==72;
}
static int write_all(int fd,const unsigned char *data,size_t size) {
    while(size){ssize_t n=write(fd,data,size);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;data+=n;size-=(size_t)n;}return 1;
}
/* Never replace a concurrently created target. Some SD filesystems reject link(). */
static int publish_missing(const char *candidate,const char *target) {
    if(link(candidate,target)==0) {
        if(unlink(candidate)) fprintf(stderr,"Kandidat konnte nicht entfernt werden: %s\n",strerror(errno));
        return 0;
    }
    int link_error=errno;
    if(link_error!=EPERM && link_error!=EOPNOTSUPP && link_error!=ENOSYS && link_error!=EXDEV) {
        fprintf(stderr,"Neues Bild konnte nicht angelegt werden (link): %s\n",strerror(link_error));return 6;
    }
#if defined(SYS_renameat2)
    /* Linux RENAME_NOREPLACE = 1; unlike plain rename(), it cannot clobber a user file. */
    if(syscall(SYS_renameat2,AT_FDCWD,candidate,AT_FDCWD,target,1u)==0)return 0;
    if(errno==ENOSYS || errno==EINVAL || errno==EOPNOTSUPP || errno==EPERM) {
        fprintf(stderr,"Dateisystem unterstützt keinen sicheren Austausch: link=%s; renameat2=%s\n",strerror(link_error),strerror(errno));return 13;
    }
    fprintf(stderr,"Neues Bild konnte nicht angelegt werden (renameat2): %s\n",strerror(errno));return 6;
#else
    fprintf(stderr,"Sicherer Austausch nicht verfügbar: link=%s; renameat2 fehlt beim Build.\n",strerror(link_error));return 13;
#endif
}
static int batch(const char *recipe,const char *rompath,const char *directory,int central){
    asset a[72];char root[PATH_MAX],path[PATH_MAX+256],priv[PATH_MAX+16],stage[PATH_MAX+32],staged[PATH_MAX+48];
    unsigned char raster[512],pal[32],bmp[78054];int i,pending=0,custom=0,status=0,fd,staged_count=0;
    struct stat st;FILE *rom=NULL;
    if(!load_assets(recipe,a)){fputs("Ungültiges Asset-Rezept.\n",stderr);return 7;}
    if(!realpath(directory,root) || (central ? strcmp(root,"/mnt/SDCARD/App/UniversalGuide/PaperMarioPAL/standard/images") != 0 : strncmp(root,"/tmp/",5) != 0) || snprintf(priv,sizeof(priv),"%s/private",root)>=(int)sizeof(priv))return 6;
    if(!lstat(priv,&st)&&(!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode)))return 6;
    for(i=0;i<72;i++){
        unsigned char *bytes;FILE *in;long len;
        if(snprintf(path,sizeof(path),"%s/%s",root,a[i].output)>=(int)sizeof(path))return 6;
        if(lstat(path,&st)){if(errno!=ENOENT)return 6;a[i].needed=1;pending++;continue;}
        if(!S_ISREG(st.st_mode))return 6;
        a[i].existed=1;a[i].before=st;
        if(st.st_size>1048576 || st.st_size<0){a[i].needed=1;pending++;continue;}
        len=(long)st.st_size;bytes=malloc((size_t)len+1);if(!bytes)return 6;
        in=fopen(path,"rb");if(!in){free(bytes);return 6;}
        if(fread(bytes,1,(size_t)len,in)!=(size_t)len){fclose(in);free(bytes);return 6;}fclose(in);
        if(is_valid_bmp(bytes,(size_t)len)){
            if(!hash_matches(bytes,(size_t)len,a[i].hash)){fprintf(stderr,"Eigenes gültiges BMP erhalten: %s\n",a[i].output);custom++;}
        } else {a[i].needed=1;pending++;}
        free(bytes);
    }
    if(!pending){printf("Keine Erzeugung nötig; %d eigene gültige Bilder erhalten.\n",custom);return custom?11:0;}
    rom=fopen(rompath,"rb");if(!rom){int e=errno;fprintf(stderr,"ROM fehlt oder nicht lesbar: %s\n",strerror(e));return e==ENOENT?2:3;}
    status=verify(rom);if(status){fclose(rom);return status;}
    if(snprintf(stage,sizeof(stage),"%s/.native-assets-XXXXXX",root)>=(int)sizeof(stage)||!mkdtemp(stage)){fprintf(stderr,"Temporäres Bildverzeichnis nicht verfügbar: %s\n",strerror(errno));fclose(rom);return 6;}
    for(i=0;i<72;i++)if(a[i].needed){
        size_t bmp_size;
        if(!strcmp(a[i].method,"pal-superblock-orthographic-bmp24")) {
            if(!render_superblock(rom,&a[i],bmp)){status=7;goto done;}bmp_size=78054;
        } else {
            if(fseek(rom,a[i].raster,SEEK_SET)||fread(raster,1,512,rom)!=512||fseek(rom,a[i].palette,SEEK_SET)||fread(pal,1,32,rom)!=32){status=3;goto done;}
            render_ci4(raster,pal,bmp,strstr(a[i].method,"zero-dpi")!=NULL);bmp_size=27702;
        }
        if(!hash_matches(bmp,bmp_size,a[i].hash)){fputs("Erzeugtes BMP stimmt nicht mit Rezept überein.\n",stderr);status=7;goto done;}
        if(snprintf(staged,sizeof(staged),"%s/%d",stage,i)>=(int)sizeof(staged)){status=6;goto done;}
        fd=open(staged,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){status=6;goto done;}
        if(!write_all(fd,bmp,bmp_size)||fsync(fd)){close(fd);unlink(staged);status=6;goto done;}
        if(close(fd)){unlink(staged);status=6;goto done;}staged_count++;
    }
    /* Check destinations again before any replacement. */
    for(i=0;i<72;i++)if(a[i].needed){
        snprintf(path,sizeof(path),"%s/%s",root,a[i].output);
        if(lstat(path,&st)==0){if(!a[i].existed || !S_ISREG(st.st_mode)||st.st_ino!=a[i].before.st_ino||st.st_size!=a[i].before.st_size||st.st_mtim.tv_sec!=a[i].before.st_mtim.tv_sec||st.st_mtim.tv_nsec!=a[i].before.st_mtim.tv_nsec){status=6;goto done;}}
        else if(errno!=ENOENT||a[i].existed){status=6;goto done;}
    }
    if(mkdir(priv,0700)&&errno!=EEXIST){status=6;goto done;}
    if(lstat(priv,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode)){status=6;goto done;}
    for(i=0;i<72;i++)if(a[i].needed){
        snprintf(path,sizeof(path),"%s/%s",root,a[i].output);snprintf(staged,sizeof(staged),"%s/%d",stage,i);
        if(a[i].existed){if(rename(staged,path)){fprintf(stderr,"Beschädigtes Bild konnte nicht ersetzt werden: %s\n",strerror(errno));status=6;goto done;}}
        else {status=publish_missing(staged,path);if(status)goto done;}
    }
    printf("%d Bilder erzeugt/repariert; %d eigene gültige Bilder erhalten.\n",pending,custom);if(custom)status=11;
done:
    fclose(rom);
    for(i=0;i<72;i++){snprintf(staged,sizeof(staged),"%s/%d",stage,i);unlink(staged);}rmdir(stage);
    (void)staged_count;return status;
}

static int central_recipe_supported(const char *path) {
    const char *expected="813c55f8d96dbfd1af0d69f7f0f7e933126bbe12f344bca834cd1464b90e7715";
    struct stat st; unsigned char *data; FILE *f; size_t n; int ok;
    if(lstat(path,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || st.st_size>65536) return 0;
    f=fopen(path,"rb"); if(!f)return 0; n=(size_t)st.st_size; data=malloc(n+1);
    if(!data){fclose(f);return 0;} ok=fread(data,1,n,f)==n && hash_matches(data,n,expected);
    free(data); fclose(f); return ok;
}
int main(int argc,char **argv) {
    FILE *rom; int status;
    if(argc==2 && !strcmp(argv[1],"--protocol-paper-mario-pal-v1")) return 0;
    if(argc==2 && !strcmp(argv[1],"--check-central-recipe")) {
        const char *recipe="/mnt/SDCARD/App/UniversalGuide/PaperMarioPAL/standard/guide_pal_badge_recipe.json";
        return central_recipe_supported(recipe)?0:7;
    }
    if(argc==5 && !strcmp(argv[1],"--batch")) return batch(argv[2],argv[3],argv[4],0);
    if(argc==3 && !strcmp(argv[1],"--central")) {
        const char *root="/mnt/SDCARD/App/UniversalGuide/PaperMarioPAL/standard/images";
        const char *recipe="/mnt/SDCARD/App/UniversalGuide/PaperMarioPAL/standard/guide_pal_badge_recipe.json";
        int lockfd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);int result;
        if(lockfd<0){fputs("Guide-Bildverzeichnis fehlt oder ist ungültig.\n",stderr);return 6;}
        if(flock(lockfd,LOCK_EX|LOCK_NB)){close(lockfd);fputs("Konvertierung läuft bereits.\n",stderr);return 12;}
        if(!central_recipe_supported(recipe)){close(lockfd);fputs("Nicht unterstütztes Bildrezept.\n",stderr);return 7;}
        result=batch(recipe,argv[2],root,1);close(lockfd);return result;
    }
    if (!((argc==3 && !strcmp(argv[1],"--verify-rom")) || (argc==4 && !strcmp(argv[1],"--power-jump")))) {
        fprintf(stderr,"Aufruf: %s --verify-rom ROM | --power-jump ROM TESTVERZEICHNIS | --batch REZEPT ROM TESTVERZEICHNIS\n",argv[0]); return 1;
    }
    rom=fopen(argv[2],"rb");
    if(!rom) { int e=errno; fprintf(stderr,"ROM fehlt oder ist nicht lesbar: %s\n",strerror(e)); return e==ENOENT?2:3; }
    status=verify(rom);
    if(!status && argc==4) status=powerjump(rom,argv[3]);
    if(!status && argc==3) puts("Unterstützte PAL-ROM verifiziert.");
    if(fclose(rom) && !status) status=3;
    return status;
}
