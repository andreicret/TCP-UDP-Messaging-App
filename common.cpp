#include <bits/stdc++.h>
#include "common.h"
#include "helpers.h"

#define SEP "/"

/**
 * @brief Separates a topic in distinct words and adds them into a vector
 * each special character '*' and '+' is treated as a distinct word
 * 
 * @param st the parsed string
 * @return vector<string> the separated elements
 */
vector<string> separate(string st) {

    char* cstr = new char[st.length() + 1];
	  DIE(!cstr, "new");

    strcpy(cstr, st.c_str());
    vector<string> separated;

    char *p = strtok(cstr, SEP);

    while (p) {
        if(p[0] == '*' || p[0] == '+') {
            do {
              // see every special character as a string
                separated.push_back(std::string(1, p[0]));
                p++;
            } while((p[0]== '*' || p[0] == '+') && p[0] != 0);

        } else
            separated.push_back(std::string(p));

        p = strtok(NULL, SEP);
    }

    delete []cstr;
    return separated;
}

/**
 * @brief Wildcard pattern matching, based on this article:
 * https://www.geeksforgeeks.org/wildcard-pattern-matching/
 * 
 * @param st the string 
 * @param pattern the pattern i want to check
 * @return true if the pattern matches the given string
 * @return false otherwise
 */
bool is_match(string st, string pattern) {

	// Pattern matching on strings
    vector <string> sep_st = separate(st);
    vector <string> sep_pat = separate(pattern);

    uint32_t sIdx = 0, pIdx = 0, lastWildcard = -1, 
    sBack = -1, nextToWild = -1;


    while (sIdx < sep_st.size()) {
        if (pIdx < sep_pat.size() &&
            (sep_pat[pIdx] == "+" || sep_pat[pIdx] == sep_st[sIdx])) {
                sIdx++;
                pIdx++;
        } else if (pIdx < sep_pat.size() && sep_pat[pIdx] == "*") {
            lastWildcard = pIdx;
            nextToWild = ++pIdx;
            sBack = sIdx;
        } else if (lastWildcard == -1) {
            return false;
        } else {
            pIdx = nextToWild;
            sIdx = ++sBack;
        }
    }

    for (size_t i = pIdx; i < sep_pat.size(); i++)
        if (sep_pat[i] != "*")
            return false;
    
    return true;
}

/**
 * @brief Receive all the expected data. Checks for truncated
 * or concatenated messages
 * 
 * @param sockfd socket used for receiving data
 * @param buffer destination of received data
 * @param len how many bytes are expected to be read
 * @return int the number of read bytes
 */
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char*)buffer;
  
    while(bytes_remaining) {
      int rcd = recv(sockfd, (char*)buffer + bytes_received, len - bytes_received, 0);
      
      if (rcd < 0)
        return -1;


      bytes_received += rcd;
      bytes_remaining -= rcd;
    }
  
  return bytes_received;
}

/**
 * @brief Send all the expected data. Checks for invalid sends
 * 
 * @param sockfd socket used for sending data
 * @param buffer data to be sent
 * @param len how many bytes are expected to be sent
 * @return int the number of sent bytes
 */
int send_all(int sockfd, void *buffer, size_t len) {

  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char*)buffer;
  
  while(bytes_remaining) {
      int rcd = send(sockfd, (char*)buffer + bytes_sent, len - bytes_sent, 0);
      
      if (rcd < 0)
        return -1;

      bytes_sent += rcd;
      bytes_remaining -= rcd;
    }

  return bytes_sent;
}
