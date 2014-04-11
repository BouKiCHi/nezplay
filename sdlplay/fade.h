

struct 
{
	int count;
	float step;
	float volume;
	
} 
fade = 
{
	0 , 0 , 1
};


static void fade_init ()
{
	fade.count  = 0;
	fade.step   = 0;
	fade.volume = 1;
}

static void fade_start( int rate , int sec )
{
	fade.count = rate * sec;
	fade.step = ((float)1)/fade.count;
	fade.volume = 1;
	
}

static void fade_stereo ( short *data , int len )
{
	// stereo
	int i = 0;
	for ( i = 0; i < len * 2; i += 2 )
	{
		data[i]   = ((float)data[i])   * fade.volume;
		data[i+1] = ((float)data[i+1]) * fade.volume;
		if (fade.count > 0)
		{
			fade.count--;
			if (fade.volume > 0)
				fade.volume -= fade.step;
			else
				fade.volume = 0;
			
			// printf("fv=%f\n",fade_volume);
			
		} else
			fade.volume = 0;
	}
}

static int fade_is_running ( void )
{
	if ( fade.count > 0 || fade.volume == 0 )
		return 1;
	
	return 0;
}

static int fade_is_end ( void )
{
	if ( fade.volume > 0 || fade.count > 0 )
		return 0;

	return 1;
}

