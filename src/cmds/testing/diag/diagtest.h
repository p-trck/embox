#ifndef DIAGTEST_H_
#define DIAGTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Add your declarations here */
extern int proc_testSpeaker(char *buf);
extern int proc_testMIC();
extern int play_sin(int freq, int volume);

#ifdef __cplusplus
}
#endif

#endif /* DIAGTEST_H_ */