#ifndef OSINFO_H_
#define OSINFO_H_

class OSInfo {
 public:
  static OSInfo* GetInstance();

 private:
  OSInfo();
  ~OSInfo();

  OSInfo(const OSInfo&) = delete;
  OSInfo& operator=(const OSInfo&) = delete;
};

#endif  // OSINFO_H_
