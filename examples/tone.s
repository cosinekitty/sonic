r = 44100;
m = 2;

program tone(y:out wave, freq:real, duration:real)
{
    y[c,i:r*duration] = { hz(freq,1,0), hz(2*freq,1,0) };
}
