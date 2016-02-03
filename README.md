# InsomniHack 2016 - Toasted

Write-up for Insomnihack 2016 Toasted challenge

Part 1 - Initial approach
Part 2 - Getting debugging information
Part 3 - Handling the slices
Part 4 - Setting debugging output

To a mighty soul
================


    Following is an explanation of what the challenge does. The challenge is run at toasted.insomnihack.ch:7200 and the objective is to read the file /flag . They also warm that "FYI Runs chrooted so
forget about your execve shellcodes." so don't try to escape the chroot,

JUST READ THE DAMN FILE :)

[Part 1 - Initial approach ]

What you download is an ARM 32-bits binary (along with the qemu-arm
needed to execute it locally). When you run it you'll be asked for a
Part 4 - Setting debugging output
passphrase.  So the first thing to look for after IDA analyzed the binary is the passphrase to log-in to the 'service'. You can easily find it in the 'strings' windows by:

1. xref-sing the string '"Passphrase : "'  which is the sting displayed when trying to login.
2. if you follow it you'll notice that if tries to read 32 bytes
3. It's strcmp(ed) against the string 'How Large Is A Stack Of Toast?\n'

That's pretty much it :) for the first part.
In case you want to check this in detail, you can examine function at 00008C36, which reads the function from the FD and compares it against the known result.

IMPORTANT: If you give it a try to the remote server during the challenge don't use Telnet client utility when trying with the real server because it sends a \x0a\x0d and the strcmp checks that only \x0a is at the end. You can use netcat or something else, not crappy telnet client :P

Part 2 - Getting debugging information
=======================================


Then you'll be promped for a slide number becuase, obviusly, this is a super cool toaster 2.0 kickstarted-founded so it's basically a cool device :)

The main function does just a couple of relevant things so now we'll proceed to the diassembly:

```
  char pBuffer[256];

  memset(pBuffer, 0, 256);
  if ( argc_cpy > 1 )
    i_show_status = 1;

  fd = open("/dev/urandom", 0);
  read(fd, &gcanary, 4u);
  canary_cpy = gcanary;

  ...
  read(fd, &i_seed, 4u);
  srandom(i_seed);
  handle_bread(pBuffer, i_show_status);
  ...
  if ( canary_cpy != gcanary )
    exit(-1);
  return 0;

```

So this is basically:

1. Setup stdin/stdout conveniently.
1. Clears the array called pBuffer where all the bread slice's status will be stored.
2. Checks argument count (main's parameter argc). You can ran it using "./qemu-arm toasted bla" (without the doublequotes) so the argc will be more than 2 and will generate verbose output.
3. Read 4 bytes from /dev/urandom to seed srandom() (this is an appropriate random source so forget any crypto sourcery).
4. Call handle_slices() at 0x000008A20 which is where the magic happens.

We'll notice that this funciton as a lot to do on the exploitation of the service but we will refer to it at the end of the write up :)

Part 3 - Handling the slices
============================

This is the function where the user input for every bread slice is handled (remember this is a toaster :)
The function has the following deifinition:
```
 int handle_slices(char *pBuffer, int i_show_status);
```

As you can see:
1. The first parameter is the pBuffer array which is a an array of 256 bytes where each slice value is going to be stored (we'll see that next).
2.  The second parameter is the status indicator which will display the array content (as a way to display debugging information) just when this parameter contains a value different than zero.
    It's worth noticing that when we run this locally we can turn this on by specifiying an additional parameter to qemu-arm as follows: ./qemu-arm tosted blabla
    This will increase the argc main's parameter and will set this value but later wel'll see that the online server does not have this flag set so wel'll do somethign about it.


At this point the we have to deal with the diassembly of the funtion handle_slices() which is the following:

```
int handle_slices(char *pBuffer, int i_show_status)
{
  pBuffer_cpy = pBuffer;
  i_show_status_cy = i_show_status;
  i_tries = 0;
  canary_cpy = gcanary;
  ...

  do
  {
    if ( overheat == 4 )
      return puts("The bread reserve tank is empty... Quitting");

    if ( i_show_status_cy )
    {
      puts("Bread status: ");
      show_bread((int)pBuffer_cpy);
    }

    puts("Which slice do you want to heat?");
    rand_res = read(0, &i_slice_n, 4u);

    if ( i_slice_n == 'q' || i_slice_n == 'x' )
      return rand_res;

    rand_res = _isoc99_sscanf(&i_slice_n, "%d", &i_input_toast_n);

    if ( rand_res && i_input_toast_n <= 255 )
    {
      rand_init = printf("Toasting %d!\n", i_input_toast_n);
      i_indexed_toast = (unsigned __int8)pBuffer_cpy[i_input_toast_n];

      cur_toast = pBuffer[toast_n];
      rval = rand();
      r1 = rval & 0xFF;
      new_toast_val = cur_toast + r1;

      if (new_toast_val <= 256)
      {
            pBuffer[toast_n] = t2 & 0xFF;
      }
      else
      {
            puts("Detected bread overheat, replacing");
            pBuffer[toast_n] = 0x0;
            ++overheat;
      }

      ++i_tries;
    
  } while ( i_tries <= 259 );

  if ( canary_cpy != gcanary )
    exit(-1);
  return rand_res;

```


The array pointer passed as the first parameter to this function is originally defined as a stack-based buffer defined in main() where its size is 256 bytes. This function basically does the following:

ENTERs a loop 259 times where it:
1. Check if the overheat variable equals 4 and leave i it does.
2. Check if local variable i_show_status_cy is not zero and display debugging iformation if it does (this is important).
3. Asks for an input number which is a bread slice number LOL
2) Uses that 4 bytes input as an index in the slices array 
3) Then it generates a random 4-bytes value, leaving only the lower byte (rand_value &= 0xFF).
4) Reads the current value at buffer[input_idx] and operates it with the rand_value (see picture and pseudocode).
5) A) If the resulting value <= 256 it performs:
      1. pBuffer[toast_n] = new_val & 0xFF
   B) When higher than 256:
      1. pBuffer[toast_n] = 0x0;
      2. Increments a overheat (if it reaches 4 then leaves).

Following is a good pseudocode description of it:

```
    cur_toast = pBuffer[toast_n];
    rval = ran___UNUSED_PARAM();
    t = 0;      //t = (rval >> 31) >> 24;
    r1 = rval & 0xF;    //t1 = r1;
    new_toast_val = cur_toast + r1;
    if (new_toast_val <= 256)
        pBuffer[toast_n] = t2 & 0xFF;
    else
        pBuffer[toast_n] = 0x0;
        overheat++;
```


Part 4 - Setting debugging output
=================================

If you enter the value -20 then you'll hit the return address of the
call to handle_bread(), meaning that you'll write the LSB byte of the
address following it so according the following disassembly:

.text:00008C82 07 F1 0C 02   ADD.W           R2, R7, #0xC
.text:00008C86 02 F1 04 02   ADD.W           R2, R2, #4
.text:00008C8A 10 46         MOV             R0, R2                ; pBuffer
.text:00008C8C 19 46         MOV             R1, R3                ; i_show_status
.text:00008C8E FF F7 C7 FE   BL              handle_bread
.text:00008C92 4D F2 8C 20+  MOV             R0, #aWellYouVeHadYo  ; "Well, you've had your toasting frenzy!\"...
.text:00008C9A 00 F0 79 FF   BL



F6FFF338  00000000  MEMORY:00000000
F6FFF33C  F6FFF378  MEMORY:F6FFF378
F6FFF340  F6FFF4B8  MEMORY:F6FFF4B8
F6FFF344  FFFFFFC0
F6FFF348  0A34362D  MEMORY:0A34362D
F6FFF34C  00000000  MEMORY:00000000
F6FFF350  12442389  MEMORY:12442389
F6FFF354  00000000  MEMORY:00000000
F6FFF358  DD0BBDFE  MEMORY:DD0BBDFE
F6FFF35C  00000000  MEMORY:00000000
F6FFF360  F6FFF368  MEMORY:F6FFF368
F6FFF364  00008C93  main+11F
F6FFF368  F6FFF604  MEMORY:F6FFF604
F6FFF36C  00000001  MEMORY:00000001
F6FFF370  DD0BBDFE  MEMORY:DD0BBDFE
F6FFF374  00000000  MEMORY:00000000
F6FFF378  00000000  MEMORY:00000000

    ~cheers, topo

