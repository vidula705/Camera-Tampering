#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "camera_tamper.h"

/********* CONFIG PARAMETERS *********/
#define     BG_LEARNING                 (0.9)
#define     DENOMINATOR                 (256 * 256)
#define     ALPHA                       ((int)(DENOMINATOR * BG_LEARNING) )
#define     BETA                        (DENOMINATOR-ALPHA)
#define     MIN_SCALING                 (1)
#define     THRESHOLD                   (50)

#define     SENSITIVITY                 (60)
#define     TAMPER_TRIGGER_THRESHOLD    (30)
#define     NO_TAMPER_MAX               (10)
#define     ABSOLUTE_ALLOWED_VARIATION  (20)

/****** ALERT ENUMS ******/
#define     NO_TAMPER_ALERT              (0)
#define     TAMPER_ATTEMPT_ALERT         (1)
#define     TAMPER_ALERT                 (2)
#define     NO_SIGNAL_ALERT              (3)

typedef unsigned char	u8;

typedef struct tag_tamper_data {
    int		width, height, channels;
    int		scale_factor;
    int		status;
    u8		*frame_full, *frame_full_gray;
    u8		*cur_frame, *prev_frame, *diff;
    u8		*cur_bg, *prev_bg;
    u8		*cur_fg;
    int		tamper_count;
    int		tamper_status;
    int		frame_count;
    int		cur_frame_mean;
    int     isFirstFrame;
} TAMPER_DATA;

void CT_BackgroundUpdate(TAMPER_DATA *context)
{
    u8 *pdiff, *p_bg, *p_bg_prev, *p_fg;
    int i,j,w,h;
    
    w = context->width/context->scale_factor, h = context->height/context->scale_factor;
    p_bg = context->cur_bg;
    p_bg_prev = context->prev_bg;
    pdiff = context->cur_frame;
    p_fg = context->cur_fg;
    // update background
    for ( i = 0; i != h; i++ )
    {
        for ( j = 0; j != w; j++, p_bg++, p_bg_prev++, pdiff++, p_fg++ )
        {
            *p_fg = (abs(*p_bg_prev - *pdiff) < ABSOLUTE_ALLOWED_VARIATION) ? 0 : 255; 
            if(!*p_fg)
                *p_bg = ( ALPHA * *p_bg_prev + BETA * *pdiff ) >> 16;
        }
    }
}


// initialize tamper detector. pass in image width, height and channels ( 3 for rgb, 1 for gray )
// returns a handle to opaque structure containing parameters of the detector
void * init_tamper_detector( int width, int height, int channels)
{
    TAMPER_DATA	*handle;
    u8			*data;
    int			image_size = width * height;

    handle = (TAMPER_DATA *)calloc( 1, sizeof( TAMPER_DATA ) );
    if ( handle == NULL ) return NULL;
    handle->width = width, handle->height = height, handle->channels = channels;
    // there are 10 images that are stored + 1 for input, assuming worst case scale = 1
    // so 1 *(w+h) for graying, 10 *(w+h)/MIN_SCALING/MIN_SCALING for the others
    data = (u8 *) calloc( 1, image_size + (image_size * 7)/MIN_SCALING/MIN_SCALING );
    if ( data == NULL ) { free( handle ); return NULL; }
    // assign the pointers to all frame storages from this single block
    handle->frame_full_gray = data; data += image_size;
    handle->cur_frame = data; data += image_size;
    handle->prev_frame = data; data += image_size;
    handle->diff = data; data += image_size;
    handle->cur_bg = data; data += image_size;
    handle->prev_bg = data; data += image_size;
    handle->cur_fg = data; data += image_size;
    handle->isFirstFrame = 1;
    handle->scale_factor = 1; 	//Keeping scaling factor to 1 as scaled Luma data is received
    return (void *)handle;
}

int deinit_tamper_detector(void *tHandle)
{
    TAMPER_DATA	*handle = (TAMPER_DATA	*)tHandle;
    if (handle == NULL)
    {
        printf("Invalid handle \n");
        return -1;
    }
        free(((TAMPER_DATA  *)tHandle)->frame_full_gray);
    free(handle);
    return 0;
}

void	sub_sample( TAMPER_DATA *context )
{
    int		i, j, w, h, step, k, l, total;
    u8		*inp, *outp, v;
    u8		*subptr, *subbase, *subrow, *subcol;
    int		sum;

    inp = context->frame_full_gray;
    outp = context->cur_frame;
    w = context->width, h = context->height, step = context->scale_factor;
    subbase = subrow = inp;
    sum = 0;
    for ( i = 0; i < h; i += step, subrow += (step * w ) )
    {
        subcol = subrow;
        for ( j = 0; j < w; j += step, subcol += step )
        {
            total = 0;
            subbase = subcol;
            // average the step sized block
            for ( k = 0; k != step; k++, subbase += context->width )
            {
                subptr = subbase;
                for ( l = 0; l != step; l++ )
                    total += *subptr++;
            }
            v = (u8) (total/step/step);
            *outp++ = v;
            sum += v;
        }
    }
    context->cur_frame_mean = sum * context->scale_factor * context->scale_factor / w /h;
    return;
}

int		corr_coeff( u8 *in1, u8 *in2, int count, int rowstep )
{
    // two image matrices in1, in2, count x count square to be corr
    //
    int		i, j;
    u8		*inp1, *inp2, *in1base, *in2base;
    long	sum_x, sum_y, sum_xy, sum_x2, sum_y2, coeff, den, den1, den2;

    inp1 = in1base = in1;
    inp2 = in2base = in2;
    sum_x = sum_y = sum_xy = sum_x2 = sum_y2 = 0;
    for ( i = 0; i != count; i++, in1base += rowstep, in2base += rowstep )
    {
        inp1 = in1base, inp2 = in2base;
        for ( j = 0; j != count; j++ )
        {
            sum_x += *inp1;
            sum_y += *inp2;
            sum_x2 += *inp1  *  *inp1;
            sum_y2 += *inp2  *  *inp2;
            sum_xy += *inp1  *  *inp2;
            inp1++, inp2++;
        }
    }
    coeff = count * count * sum_xy - ( sum_x * sum_y );
    den1 = count * count * sum_x2 - sum_x * sum_x;
    den1 = (long)sqrt(den1 * 1.0 );
    den2 = ( count * count * sum_y2 - sum_y * sum_y );
    den2 = (long)sqrt( den2 * 1.0 );
    den  = den1*den2;
    if ( !den )
    {
        if(den1 != 0 || den2 != 0)
        {
            return 0;
        }
        else if(den1 == 0 && den2 == 0)
        {
            if(sum_y != sum_x)
                return 0;
            else
                return 100;
        }
    }
    //scale it between 0-100 by diving by 2 and adding 50.
    // the original range is between -100 to +100
    return (int) ((( coeff * 100.0 / den )/2)+50);
}

#define		CORR_SIZE	8
int	CT_FindTamper( TAMPER_DATA *context )
{
    int		i, j, w, h, step;
    u8		*in_1, *in_2;
    int		corr_coeff_total, CT_TamperCount, coeff;

    in_1 = context->cur_frame;
    in_2 = context->cur_bg;
    w = context->width, h = context->height, step = context->scale_factor;
    w = w/step, h = h/step;

    CT_TamperCount = corr_coeff_total = 0;
    for ( i = 0; i < h; i += CORR_SIZE )
    {
        for ( j = 0; j < w; j += CORR_SIZE )
        {
            coeff = corr_coeff( in_1 + i * w + j,  in_2 + i * w + j, CORR_SIZE, w );
            CT_TamperCount++;
            corr_coeff_total += coeff;
        }
    }
    
    return corr_coeff_total/CT_TamperCount;

}


int ipf_tamper_processing( void *tHandle, unsigned char *inimg)
{
    TAMPER_DATA		*context = (TAMPER_DATA	*)tHandle;
    int				w, h, bg_upd = 0;
    int				CT_TamperCount = 2000;
    if (context == NULL) {
        printf("Invalid handle \n");
        return -1;
    }
    
    w = context->width/context->scale_factor, h = context->height/context->scale_factor;
    
    // Copy the Y data to grey frame
    memcpy( context->frame_full_gray, inimg, context->width * context->height );
    
    if(context->isFirstFrame)
    {
        memcpy( context->cur_bg, inimg, w * h );
        memcpy( context->prev_bg, inimg, w * h );
        context->isFirstFrame = 0;
    }

    // subsample to reduce size --> This is required for curr_frame_mean
    sub_sample( context ); 
    memcpy( context->prev_frame, context->cur_frame, w * h );

    CT_BackgroundUpdate(context);
    
    // now compute the correlation
    CT_TamperCount = CT_FindTamper( context );
        //printf("CT_TamperCount = %d            tamper_count = %d \n",CT_TamperCount,context->tamper_count);
    memcpy( context->prev_bg, context->cur_bg, w * h );
    if ( CT_TamperCount < SENSITIVITY )
    { 
        if(context->tamper_count < 200)
        {
            context->tamper_count++;
        }
        else
        {
            printf("----------- UPDATING BG RESTARTED -------");
            context->tamper_count = 0;
            CT_TamperCount =0;
        }
    }
    else 
    { 
                //if(context->tamper_count > 0)
            //context->tamper_count--;
        context->tamper_count = 0;
    }
    if ( context->tamper_count < NO_TAMPER_MAX )
    { 
        context->tamper_status = NO_TAMPER_ALERT;   
    //	printf( "Camera OK \t Tamper count =%d \n",context->tamper_count);
    }
    else if ( context->tamper_count > NO_TAMPER_MAX && context->tamper_count <= TAMPER_TRIGGER_THRESHOLD )
    { 
        context->tamper_status = TAMPER_ATTEMPT_ALERT; 
        //printf( "Tampering Attempt \t Tamper count =%d\n",context->tamper_count);
    }
    else if ( context->tamper_count > TAMPER_TRIGGER_THRESHOLD ) 
    {
        context->tamper_status = TAMPER_ALERT; 
        printf( "TAMPERED \t Tamper count =%d\n",context->tamper_count);
    }
    return context->tamper_status;
}

