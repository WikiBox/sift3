# sift3
Repository sifter and hardlinker - Double Deep

GPL3 Software

sift3 - An improved file sifter

sift3 is a utility, similar to sift, that allows you to automatically sift source folders for items (files or folders) and selectively move or hard link these items to subfolders in a destination folder. 

This is similar to the older WikiBox/sift but sift3 is a total rewrite with much improved matching and features.

Now sift3 supports varable depth repo and dest. Matching is by complete word or optionally free. CamelCase is detected and used.

## Usage

    sift3 Copyright Anders Larsen 2020 gislagard@gmail.com
    
    Hardlink items in repo to matching folders at dest.
    
    Matching rules:
    
      Items match once and in order.
      Matches are not case sensitive for ASCII.
      Suffix '...' in folder names creates a parent folder.
      Lower case words in parent folders are not matched.
      Use any of ',(); for alt match in not parent dest.
      Use _underscore_ in dest to ignore word start/stop.
      
    Usage: " << argv[0] << " [options] repo dest
    
    Options:
    
        -c or --clear    Clear existing items in dest before sift.
        -m or --missing  Log not matching in repo to dest/missing.txt.
        -v or --verbose  Increase verbosity. Max is 3: -vvv
 
 ## Compile and install
 
sift3 is a very small utility, consisting of a singe C++ source file, written using only Standard C++ 17 and without any dependencies, so no make or similar is needed. But a recent compiler that supports C++ 17 is needed. The normal gcc 8.4.0-3 that comes with Ubuntu 20.04 works fine.

This command creates an executable, sift3, ready to use or move into some folder in the path:

    g++ -O3 -std=c++17 sift3.cpp -o sift3
    
## Details and examples

This documentation is just started, much more documentation is needed and will be added over time.

### Repository - repo

A repo is simply a folder holding a number of items to be sifted, subfolders or normal files. This can be downloaded files or photos or whatever. Subfolders with the suffix "..." are used as "parent repo subfolders" that also can hold items, in any depth.

The filenames of the parent folders and the items themselves is combined to provide a description of each item.

Example:

    1. /repo/old/Movies .../2001 - A Space Odessey - Science Fiction - Stanley Kubrik (1968)/2001.mkv

Here we have a parent folder "Movies" and an item consisting of the subfolder "2001 - A Space Odessey - Stanley Kubrik (1968)". The description of the item is the combination of the parent folder name and the foldername. However, words in parent subfolders that are not capitalized (Title Case) are ignored, as are any of "(),;". So the description used by sift3 consists of the following tokens:

    [Movies] [2001] [-] [A] [Space] [Odessey] [-] [Science] [Fiction] [-] [Stanley] [Kubrik] [1968]
    
Sometimes CamelCase is used to separate words, instead of space. This is detected and used by sift3. So the filename:

    2. Bladerunner ScienceFiction.mkv
    
Would be used as the following tokens:

    [Bladerunner] [Science] [Fiction]

### Destination - dest

A destination is simply a folder with a number of empty "dest" subfolders. Subfolders with the suffix "..." are used as "parent dest subfolders" that also can hold dest subfolders, in any depth. 

Examples:

    3. /dest/Movies .../by director .../Stanley Kubrik/
    4. /dest/Movies .../by genre .../Science Fiction/
    5. /dest/Movies .../by genre .../Comedy
    6. /dest/Movies .../by year .../1968/
    7. /dest/Movies Sci_ Fi_/
    8. /dest/Sci_ Fi_ Movies/
    9. /dest/Sci_ Fi_ movies/
    
The parent dest folder names and the dest folder names are combined by sift3 to create a series of "search" tokens. However, words in parent dest folders that are not capitalized (Title Case) are ignored, as are any of "(),;". 

Several variants can be specified in the dest folder (not in a parent dest folder) by separating the variants with any of "(),;". In addition the underscore character \_ can be used to specify "free" search without having to match exactly the start or end of a word.

### The sift - hardlinking from repo to dest

sift3 reads in all folders in dest to memory and for each variant creates a set search tokens. Then sift3 walks through the repo, and for each item tries to find a matching set of search tokens from dest. 

The search tokens from dest need to match the repo description once, in strict order. If all search tokens from dest match, then the item from repo is hardlinked into the matching subfolder in dest.

Examples 3, 4, 6, 7 and 9 will match example 1.


__Warning: sift3 is intentionally designed to delete/clear whole folder trees without asking for any confirmation. Be careful. Backup your data.

__Warning: You should understand what hardlinking means and how it works. The files in dest are not copies, they are the actual same files as the files in repo. Don't edit the contents, unless you want it propagated to all the same hardlinked files.

### Clear

If you rename or restructure things you may want to remove old hardlinked itemes in dest. Run sift3 with the commandline option --clear and old hardlinks will be removed.

Parent dest folders and empty dest folders will remain.

Warning: sift3 will NOT ask for any confirmation before it clears out dest.

Tip: To get an empty dest just run a sift3 with --clear using an empty folder as repo. This is very useful if you want to backup dest. If you backup repo and an empty dest, you have a full backup.

### Missing - missing.txt

You may want to find out what items in the repo are not matched in dest, so you can add new dest folders to handle that. Run sift3 with the commandline option --missing. Then a file dest/missing.txt will be created with all items not matched from the repo.

### Performance and no progress indicator - verbosity

Hardlinking is fast. Each repo item is compared to dest search sets held in RAM. But this is in essence a brute force nested linear search, every single dest folder is tested against every single item in repo, wich is very bad for performance. But for a moderate size repo and a moderate size dest, on a local fast filesystem, sift3 runtimes are typically measured in seconds or single minutes. Typically it is the filesystem speed that is the bottleneck, not the processing power.

But if you use sift3 on a remote filesystem on a NAS, over WiFi or slow Ethernet and both repo and dest is large, sift3 can take a long time to finish. I did some "worst case" testing over wifi to a NAS running on a RPi4 with mergerfs with very big repo and dest and runtimes could be close to an hour. Around half of that time was spent deleting old hardlinks using --clear.

If you need to improve performance, split your data into several smaller repo and dest. Run on a local filesystem, not remotely. In other words, run sift3 on the NAS itself, not on the remote networked shared filesystem.

If you want some indication that sift3 is working and hasn't hung, you can increase the verbosity using the --verbosity commandline option. Each time you use the option the verbosity level is increased one step.

Verbosity 0: sift3 is silent

Verbosity 1: sift3 tells when it shifts between reading (and celaring) dest and reading repo.

Verbosity 2: sift3 tells you about what it does with every item in dest and repo.

Verbosity 3: for debugging, not recommended...

Verbosity 2 gives a good indication of the pace of sift3.

Instead of using --verbosity multiple times you can use -v muliple times or -vv or -vvv.
