<?php
//TITLE=NES movie file format converter

$title = 'NES movie fileformat converter';
$progname = 'nesmock';

function usagetext($prog)
{
  exec('/usr/local/bin/'.$prog.' --help', $kk);
  $k='';foreach($kk as $s)$k.="$s\n";
  return $k;
}
 
$text = array(
   '1. Purpose' => "

This program converts different format NES movies to other formats.

", 'usage:1. Usage' => "

<pre class=smallerpre>".htmlspecialchars(usagetext('nesmock'))."</pre>

Offsets and delays are usually needed because different emulators
have different timings in various details.
Adjusting them is usually trial&amp;error work.
 <p>
If you are doubting whether this program actually works at all, try converting
Super Mario Bros movies. I have had complete success with them.
 <p>
Hint: Nesmock can be used for editing FCM files by first
converting them to FMV (which is easier to edit than FCM)
and then back to FCM.

", '1. Copying' => "

nesmock has been written by Joel Yliluoma, a.k.a.
<a href=\"http://iki.fi/bisqwit/\">Bisqwit</a>,<br>
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/licenses.html#GPL\">General Public License</a> (GPL).
 <p>
If you are an emulator developer, you are welcome to hack this source
code and add more format support to the code.<br>
These things are currently missing:
<ul>
 <li>VirtuaNES movie write support</li>
 <li>Nesticle movie read support</li>
 <li>Nesticle movie write support</li>
 <li>Nintendulator movie write support</li>
 <li>FCEU savestate read support</li>
 <li>FCEU savestate write support</li>
 <li>VirtuaNES savestate read support</li>
 <li>VirtuaNES savestate write support</li>
 <li>Famtasia savestate read support</li>
 <li>Famtasia savestate write support</li>
</ul>

", '1. Requirements' => "

GNU make and C++ compiler is required to recompile the source code.<br>
For the program (including the Windows binary), you need a commandline.<br>

", '1. Thoughs' => "

Different emulators emulate differently. This is a fact.
For this reason, usually straight conversions of the movies
don't just work.
 <p>
It is unfortunately impossible to guess from the movie file alone where
would the emulator need more / less frames than the other emulator.
 <p>
A more elaborate conversion could be made by running the two emulators
simultaneously and keeping track of their status, and when the other goes
desync, backtrack and bruteforce the combination that puts it back to track.
This kind of method is however extremely difficult to implement. And it
still might be impossible - if the game does calculations based on scanline
counters, there could be no way to make them match.

");
include '/WWW/progdesc.php';
