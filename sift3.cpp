/* 
    sift3 - Copyright Anders Larsen 2020 Gislagard@gmail.com

    For docs, see: https://github.com/WikiBox/sift3

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <cctype>

namespace fs = std::filesystem;
using namespace std;

int verbose;
bool clear, missing;
ofstream ostrm_missing;

vector<pair<fs::path, string> > repo_items;
vector<pair<fs::path, vector<string> > > dest_items;

void tokenize(string str, const string& delimiter, vector<string> &tokens)
{
    size_t prev = 0, pos = 0, len = str.length();

    do
    {
        pos = str.find_first_of(delimiter, prev);
        
        if (pos == string::npos) 
            pos = len;   
        
        string tok = str.substr(prev, pos - prev);

        if (!tok.empty())
            tokens.push_back(tok);
        
        prev = pos + 1;

    } while (pos < len && prev < len);
}

/*
 * Recursive hardlink (copy file w. hardlink&recursion) in the stdlib seems to be broken
 */

void recursive_hardlink(fs::path src, fs::path dst)
{
    std::error_code ec;

    if (fs::is_directory(src))
    {
        if(verbose > 2) cout << "create directory: " << dst << endl;

        fs::create_directory(dst, ec);     // mkdir
        if (ec && (verbose > 2))
            cerr << "Create directory warning: " << ec.message() << " " << dst.string() << endl;
        
        for(auto& new_src : fs::directory_iterator(src))
            recursive_hardlink(new_src, dst / new_src.path().filename());      
    }
    else if (fs::is_regular_file(src))
    {
        if(verbose > 2) cout << "hardlink file: " << src << endl;
        
        // Skip if dst exists
        if(fs::exists(dst))
            return;

        fs::create_hard_link(src, dst, ec);
        if (ec && (verbose > 2))
            cerr << "Create hardlink warning: " << ec.message() << " " << dst.string() << endl;
    }
}

/*
 * Find word breaks to use for valid starts and stops of word matches
 */

void build_start_stop(string& s, vector<bool>& start, vector<bool>& stop)
{
    size_t len = s.length();

    char pre, cur, suc;

    for(int i = 0; i < len; i++)
    {
        if((i + 1) == len) 
            suc = '\0'; 
        else 
            suc = s[i + 1];

        if(i == 0) 
            pre = '\0';
        else 
            pre = cur;

        cur = s[i];

        // Is cur a valid start of a word?
        start.push_back(((isalpha(cur) && !isalpha(pre)) ||   // _[a]
                         (isupper(cur) &&  islower(pre))));   // a[A]
        
        // Is cur a valid stop of a word?
        stop.push_back(((isalpha(cur) && !isalpha(suc)) ||    // [a]_
                        (islower(cur) &&  isupper(suc))));    // [a]A
    }
}

size_t find_word_case_insensitive(std::string_view s, size_t spos, std::string_view p)
{
    auto it = std::search
    (
        s.begin() + spos, s.end(),
        p.begin(), p.end(),
        [](char cs, char cp) { return std::toupper(cs) == std::toupper(cp); }
    );

    if (it == s.end())
        return string::npos;

    return it - s.begin(); 
}

/*
    Search s for the strings in p, in order. If all are found, return true. 
    Recursive backtracking.

         s : string to search.
      spos : position in s to search from.
         p : strings to match.
      ppos : position in p to search from.
    wstart : flags match valid start of word.
     wstop : flags match valid stop of word..

    + at start/end of string to match turn off checks for start/stop word breaks. 
*/

bool match(string& s, size_t spos, const vector<string>& p, size_t ppos, const vector<bool>& wstart, const vector<bool>& wstop)
{
    if (spos == s.length() && (ppos != p.size()))   // Exhausted s (searched string) but not p (patterns to search for in s)
        return false;                               
    else if (ppos == p.size())                      // Exhausted p
        return true;

    std::string_view q = p[ppos];

    // Check if first char is UPPER CASE - Means Word match required - otherwise more relaxed string match
    // bool test = isupper(q.front()); 
    // bool test_start = test, test_stop = test;

    bool test_start = true, test_stop = true;

    if(q.front() == '_')
    {
        test_start = false;
        q.remove_prefix(1);
    }

    if(q.back() == '_')
    {
        test_stop = false;
        q.remove_suffix(1);
    }
   
    size_t qlen = q.length();
    size_t pos = 0;

    while(string::npos != (pos = find_word_case_insensitive(s, spos, q)))
        if((!test_start || wstart[pos]) && (!test_stop || wstop[pos + qlen - 1]))
            return match(s, pos + qlen, p, ppos + 1, wstart, wstop);
        else
            spos = pos + qlen;
        
    return false;
}

inline bool has_suffix(std::string const & s, std::string const & suffix)
{
    if (suffix.length() > s.length()) 
        return false;

    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

/*
 * Iterate over everything in repo and sift one item after another.
 * repo_info holds all words from previous recursion to use in a match
 */

void sift_repo(const fs::path repo_folder, const string repo_info = "")
{
    for(auto& repo_folder_item : fs::directory_iterator(repo_folder))
    {
        string filename = repo_folder_item.path().filename().string();    

        if(!has_suffix(filename, "...") ||       // has not suffix ... 
           !fs::is_directory(repo_folder_item))  // or is not a file
        {
            string new_repo_info = repo_info;

            if(!new_repo_info.empty())
                new_repo_info.append(" ");
                        
            new_repo_info.append(filename);
    
            vector<bool> stop, start;

            build_start_stop(new_repo_info, start, stop);

            bool have_match = false;

            for(const auto& [dest, words] : dest_items)
            {
                size_t next = 0, pos;

                if (match(new_repo_info, 0, words, 0, start, stop))
                {
                    const fs::path dst = dest / filename;
                
                    if(verbose > 0)
                        cout << " Linking: " << repo_folder_item.path() << "\n"
                             << "    with: " << dst << endl;

                    recursive_hardlink(repo_folder_item, dst); 
                    have_match = true;                     
                }
            }
                        
            if(!have_match)
            {
                if(missing)
                    ostrm_missing << repo_folder_item.path() << endl;
           
                if(verbose > 0)
                    cout << "No match: " << repo_folder_item.path() << endl;
            }
        }
        else // has suffix ... and is a folder
        {
            vector<string> words;
            string new_repo_info = repo_info;

            // Erase "..." suffix
            filename.erase(filename.length() - 3, string::npos);

            tokenize(filename, " ", words);
            
            // Erase all words starting with alpha but not capitalized
            words.erase(std::remove_if(words.begin(), words.end(), 
                [](string s){return isalpha(s.front()) && !std::isupper(s.front());}), words.end());

            // Reconstruct filename into new_repo_info
            for(auto& word : words)
            {
                if(!new_repo_info.empty())      // pedantic
                    new_repo_info.append(" ");

                new_repo_info.append(word);
            }

            if(verbose > 2) cout << "new_repo_info used: " << new_repo_info << endl;

            sift_repo(repo_folder_item.path(), new_repo_info);
        }
    }
}

/*
 * Read all destination folders, store search tokens only if Capitalized first letter - recursive
 * dest_info holds all search terms
 */

void read_dest(const fs::path dest_folder, const string dest_info = "")
{
    for (auto& dest_folder_item : fs::directory_iterator(dest_folder))
    {
        string filename = dest_folder_item.path().filename().string();

        if(fs::is_directory(dest_folder_item) && !has_suffix(filename, "..."))  // This is a destination folder!
        {
            string new_dest_info = dest_info;

            if(!new_dest_info.empty())
                new_dest_info.append(" ");

            new_dest_info.append(filename);
                
            if(std::error_code ec; clear)
            {
                for(auto& item: fs::directory_iterator(dest_folder_item))
                {
                    if(verbose > 1)
                        cout << "Deleting: " << item.path().string() << endl;
                    
                    fs::remove_all(item.path(), ec);
                }
            }

            vector<string> variants;

            tokenize(new_dest_info, ",();", variants);

            for (auto& var : variants)
            {
                vector<string> words;

                tokenize(var, " ", words);

                // Remove all words not capitalized
                words.erase(std::remove_if(words.begin(), words.end(), 
                    [](string s){return isalpha(s.front()) && !std::isupper(s.front());}), words.end());

                if (!words.empty())
                {
                    dest_items.push_back(make_pair(dest_folder_item, words));       
                        
                    if(verbose > 2)
                    {
                        cout << "Dest tokens used for " << dest_folder_item.path().string() << endl;
                        for(auto& s : words)
                            cout << "[" << s << "] ";

                        cout << endl;
                    }
                }
            }
        }
        else if (fs::is_directory(dest_folder_item) && has_suffix(filename, "...")) 
        {
            string new_dest_info = dest_info;

            if(!new_dest_info.empty())
                new_dest_info.append(" ");

            filename = filename.substr(0, filename.length() - 3);  // erase ...

            string space_these = ",();";  // Variants not allowed in parent folder

            for(auto& c : filename)
                if(space_these.find(c) != string::npos)
                    c = ' ';

            new_dest_info.append(filename);

            read_dest(dest_folder_item.path(), new_dest_info);
        }
    }
}

int main(int argc, char** argv)
{
    string repo, dest;

    bool syntax_error = false;
    
    clear = missing = false;
    verbose = 0;

    for (int i = 1; i < argc; i++)
    {
        string arg = string(argv[i]);

        if ((arg == "-c") || (arg == "--clear"))
            clear = true; 
        else if ((arg == "-m") || (arg == "--missing"))
            missing = true;
        else if ((arg == "-v") || (arg == "--verbose"))
            verbose++;
        else if ((arg == "-vv"))
            verbose+=2;
        else if ((arg == "-vvv"))
            verbose+=3;
        else if (i == (argc - 2))
            repo = arg;
        else if (i == (argc - 1))
            dest = arg;
        else
        {
            syntax_error = true;
            break;
        }
    }

    if(syntax_error || !(fs::exists(repo) && fs::exists(dest)))
    {            
        cerr << "sift3 Copyright Anders Larsen 2020 gislagard@gmail.com\n\n"
             << "This program is free software under the terms of GPLv3.\n\n"
             << "Hardlink items in repo folder to matching subfolders in dest.\n\n"
             << "Matching rules: \n"
             << "  Items match only once and in strict order.\n"
             << "  Matches are not case sensitive for ASCII.\n"
             << "  Suffix '...' in folder names creates a parent folder.\n"
             << "  Lower case words in parent folders are not matched.\n"
             << "  Use any of ',(); for alternative match in not parent dest.\n"
             << "  Full words must match. CamelCase is detected if used.\n"
             << "  Use _underscore_ in dest to disable full word start/stop.\n\n"
             << "Usage: sift3 [options] repo dest\n\n" 
             << "Options:\n\n"
             << "    -c or --clear    Clear existing items in dest before sift.\n"
             << "    -m or --missing  Log not matching in repo to dest/missing.txt.\n"
             << "    -v or --verbose  Increase verbosity. Max is 3: -vvv\n" 
             << endl;

        return -1;
    }

    if(verbose > 0 && clear)
        cout << "Reading and clearing " << dest << endl; 
    else if(verbose > 0)
        cout << "Reading " << dest << endl; 
    
    read_dest(dest);

    if(missing)
        ostrm_missing.open(fs::path(dest) / "missing.txt", std::ios::binary);

    if(verbose > 0)
        cout << "Sifting " << repo << endl;

    sift_repo(repo);

    if(missing)
    {  
        ostrm_missing.close();
    
        if(verbose > 0) 
            cout << dest << "/missing.txt closed" << endl;
    }

    return 0;
}
