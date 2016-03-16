#include <unistd.h>
#include <stdio.h>
#include "soft_crc32.h"
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <vector>
#include <string>

using namespace std;

bool map_file_to_memory(int fd,size_t * length_,uint8_t ** data_) {
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1 ) {
      cout << "(ERROR)" << "fstat " << fd;
      return false;
    }

    *length_ = file_stat.st_size;

    *data_ = static_cast<uint8_t*>(mmap(NULL, *length_, PROT_READ, MAP_SHARED, fd, 0));
    if (*data_ == MAP_FAILED)
      cout <<"[ERROR]" << "mmap " << fd;

    return *data_ != MAP_FAILED;
}

#define IGNORE_EINTR(x) ({ \
  typeof(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0; \
    } \
  } while (0); \
  eintr_wrapper_result; \
})


int WriteFileDescriptor(const int fd, const char* data, int size) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size; bytes_written_total += bytes_written_partial) {
    bytes_written_partial = write(fd, data + bytes_written_total, size - bytes_written_total);
    if (bytes_written_partial < 0)
      return -1;
  }

  return bytes_written_total;
}

int WriteFile(const char* filename, const char* data, int size) {
  int fd = creat(filename, 0666);
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size);
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_written;
}


int AppendToFile(const char* filename, const char* data, int size) {
  int fd = open(filename, O_WRONLY | O_APPEND);
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size);
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_written;
}

void print_help(void)
{
    cout << "add_crc src_file_name dest_file_name" << endl;
}


int main (int argc,char ** argv)
{
    int ret;

    if(argc != 3) {
        cout << " arg error, arg format :\n" <<endl;
        print_help();
        return -1;
    }

    cout << "src_file_name is : "<< argv[1]<<"\n";
    cout << "dest_file_name is : "<< argv[2]<<endl;

    soft_crc32_init();

    uint8_t * map_addr;
    size_t length;

    int src_fd = open(argv[1],O_RDONLY);
    if(src_fd < 0) {
        cout << "open src file error";
        return -1;
    }

    bool result = map_file_to_memory(src_fd,&length,&map_addr);
    if(!result) {
        close(src_fd);
        cout << "map file fail \n";
        return -1;
    }

    uint32_t crc = soft_crc32(map_addr,length,0);

    cout << "crc is "<< crc << endl;
    //create a new file, then write content back
    
    int bytes_written = WriteFile(argv[2],(const char *)map_addr,length);
    if(bytes_written != length) {
        ret = -1;
        cout << "write fail \n";
        goto handle_error;
    }

    bytes_written = AppendToFile(argv[2],(const char*)&crc,sizeof(uint32_t));
    if(bytes_written != sizeof(uint32_t)) {
        ret = -1;
        cout << "write crc fail " << endl;
        goto handle_error;
    }

    ret = 1;

handle_error:
    munmap(map_addr,length);
    close(src_fd);
    return ret ;
}
