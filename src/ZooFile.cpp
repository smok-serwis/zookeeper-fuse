/*
 * Copyright 2016-2021 Kyle Borowski & Piotr Maślanka
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * File:   ZooFile.cpp
 * Author: kyle & Piotr Maślanka
 *
 * Created on July 27, 2016, 7:52 PM
 */

#include <string.h>
#include <string>
#include <iostream>
#include <unordered_set>
#include "ZooFile.h"

// @todo add cache invalidation. Now the entries are kept for all eternity
unordered_set<string> for_sure_files, for_sure_directories;

const size_t ZooFile::MAX_FILE_SIZE = 262144;
bool is_hybrid_mode = false;


string ZooFile::getContentAndSetWatch() const {

    Stat stat;
    char content[MAX_FILE_SIZE];
    int contentLength = MAX_FILE_SIZE;

    memset( content, 0, MAX_FILE_SIZE );
    memset( &stat, 0, sizeof(stat) );

    int rc = zoo_get(handle_, path_.c_str(), 1, content, &contentLength, &stat);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred getting the contents of file: " + path_, rc);
    }

    string retval;
    if (contentLength > 0) {
        retval = string(content, contentLength);
    } else {
        retval = "";
    }

    return retval;

}

void ZooFile::markAsDirectory() const {
    unordered_set<string>::iterator it = for_sure_files.find(path_);
    if (it == for_sure_files.end()) {
        // Ie. it's not a file
        for_sure_directories.insert(path_);
    }
}
void ZooFile::markAsFile() const {
    unordered_set<string>::iterator it = for_sure_directories.find(path_);
    if (it == for_sure_directories.end()) {
        // Ie. it's not a directory
        for_sure_files.insert(path_);
    }
}

ZooFile::ZooFile(zhandle_t* handle, const string &path) :
handle_(handle),
path_(path) {

}

ZooFile::~ZooFile() {

}

bool ZooFile::exists() const {
    int rc = zoo_exists(handle_, path_.c_str(), 0, NULL);
    if (rc == ZNONODE) {
        return false;
    }
    if (rc != ZOK) {
        throw ZooFileException("An error occurred checking the existence of file: " + path_, rc);
    }
    return true;
}

bool ZooFile::isDir() const {
    if (!is_hybrid_mode) {
        return !getChildren().empty();
    }

    if (path_ == "/") {
        return true;
    }
    if (for_sure_files.find(path_) == for_sure_files.end()) {
        if (for_sure_directories.find(path_) == for_sure_directories.end()) {
            // If it is empty...
            if (getChildren().empty()) {
                // If it has any content
                return (getLength() == 0);
            }
            // else is a directory, since it has children
            return true;
        }
        return true;
    }
    return false;
}

vector<string> ZooFile::getChildren() const {
    vector<string> retval;
    String_vector children;

    int rc = zoo_get_children(handle_, path_.c_str(), 0, &children);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred getting children of file: " + path_, rc);
    }

    for (int i = 0; i < children.count; i++) {
        retval.push_back(children.data[i]);
    }
    deallocate_String_vector(&children);
    return retval;
}

bool ZooFile::hasChildren() const {
    vector<string> children = getChildren();
    return (children.size() > 0);
}

string ZooFile::getContent() const {
    Stat stat;
    char content[MAX_FILE_SIZE];
    unsigned int contentLength = MAX_FILE_SIZE;

    memset( content, 0, MAX_FILE_SIZE );
    memset( &stat, 0, sizeof(stat) );

    int rc = zoo_get(handle_, path_.c_str(), 0, content, &contentLength, &stat);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred getting the contents of file: " + path_, rc);
    }

    string retval;

    if (contentLength > MAX_FILE_SIZE) {
        int previousLength = contentLength;
        char * file_contents = new char[previousLength];
        int rc = zoo_get(handle_, path_.c_str(), 0, file_contents, &contentLength, &stat);
        if (rc != ZOK) {
            delete file_contents;
            throw ZooFileException("An error occurred getting the contents of file: " + path_, rc);
        }
        if (previousLength != contentLength) {
            // The file was modified in-transit
            return getContent();
        }
        retval = string(file_contents, contentLength);
        delete file_contents;
    } else {
        if (contentLength > 0) {
            retval = string(content, contentLength);
        } else {
            retval = "";
        }
    }
    return retval;
}

void ZooFile::setContent(string content) {
    int rc = zoo_set(handle_, path_.c_str(), content.c_str(), content.length(), -1);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred setting the contents of file: " + path_, rc);
    }
}

void ZooFile::create() {
    int rc = zoo_create(handle_, path_.c_str(), NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred creating the file: " + path_, rc);
    }
}

size_t ZooFile::getLength() const {
    return (getContent().length());
}

void ZooFile::remove() {
    int rc = zoo_delete(handle_, path_.c_str(), -1);
    if (rc != ZOK) {
        throw ZooFileException("An error occurred deleting the file: " + path_, rc);
    }
    for_sure_files.erase(path_);
    for_sure_directories.erase(path_);
}

void enableHybridMode() {
    is_hybrid_mode = true;
}
