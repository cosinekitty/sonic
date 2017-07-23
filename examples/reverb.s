/*
     reverb.s

     This is a sample program written in Sonic.
     This program performs audio reverberation.
*/

program reverb (
    inWave: wave,
    outWave: wave,
    delay: real,
    decay: real,
    extend: real )
{
	var fred=1+3, asdf: integer;

    outWave[c, i: inWave.n + r*extend] =
        inWave[c,i] +
        decay * outWave[c, i - r*delay];
}
