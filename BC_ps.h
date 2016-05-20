struct uproc {
  int pid;
  int uid;
  int gid;
  int ppid;
  char state[12];
  int size;
  char name[12];
  int priority;
};
