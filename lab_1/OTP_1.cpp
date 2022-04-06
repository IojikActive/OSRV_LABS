#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <fstream>
#include <fcntl.h>
#include <cstring>
#include <vector>


static int _test_count_Thread = 0;

using std::cout;

enum states{
	ERROR_CREATE_THREAD = 100,
	ERROR_BARRIER,
	ERROR_BARRIER_DESTROY,
	ERROR_WAIT_BARRIER,
	ERROR_JOIN_THREAD,
	ERROR_FILE_R_OPEN,
	ERROR_FILE_W_OPEN,
	ERROR_FILE,
	SUCCESS = 0
};
//pthread_barrier_t barrier;

struct programParam{
	size_t a;
	size_t c;
	size_t m;
	size_t seed; // x0
	char* inputFilePath;
	char* outputFilePath;
};

struct keygenParam{
	size_t a;
	size_t c;
	size_t m;
	size_t seed; // x0
	size_t sizeKey;
};

struct cryptParam
{
	char* msg;
	char* key;
	char* outputText;
	size_t size;
	size_t downIndex;
	size_t topIndex;
	pthread_barrier_t* barrier;
};

void* keyGenerate(void* params){ // подготовка к генерации ЛКГ и генерация ЛКГ
	keygenParam *parametrs = reinterpret_cast<keygenParam *>(params);

	size_t a = parametrs->a;
	size_t m = parametrs->m;
	size_t c = parametrs->c;
	size_t sizeKey = parametrs->sizeKey;


	int* buff = new int[sizeKey/sizeof(int)+1];
	buff[0] = parametrs->seed;


	for(size_t i = 1; i < sizeKey/sizeof(int) + 1 ; i++){
		buff[i]= (a * buff[i-1] + c) % m; // ЛКГ генерация чисел
	}
	
	for (size_t i = 0;i<sizeKey;i++){
		cout << buff[i]<<",";
	}
	cout <<"BUFF END\n\n";


	return reinterpret_cast<char *>(buff);
};

void* crypt(void * cryptParametrs)
{
	cout  << "\n"<< _test_count_Thread++ << "\n";
	int status = 0;

	cryptParam* param = reinterpret_cast<cryptParam*>(cryptParametrs); //Жестко указываем компилятору тип
	size_t topIndex = param->topIndex;
	size_t downIndex = param->downIndex;

	while(downIndex < topIndex)
		{
			param->outputText[downIndex] = param->key[downIndex] ^ param->msg[downIndex];
			// cout <<"#Thread: "<< _test_count_Thread <<"\n";
			// cout << "DownIndex" << downIndex<<":"<<param->msg[downIndex];
			cout << param->msg[downIndex];

			downIndex++;
		}
		// cout << "\n\n\n";

	status = pthread_barrier_wait(param->barrier); 
		if(status != PTHREAD_BARRIER_SERIAL_THREAD && status != 0)
			{
			std::cout << "problem with pthread_barrier_wait";
			exit(ERROR_WAIT_BARRIER);
			}
			return 0;
}

int main (int argc, char **argv) {
	//#1
	int c;
	programParam progParam;
	while ((c = getopt(argc, argv, "i:o:a:c:x:m:")) != -1) { // Разбор флагов
		switch (c) {
		case 'i':
			printf ("option i with value '%s'\n", optarg);
			progParam.inputFilePath = optarg;
			break;
		case 'o':
			printf ("option o with value '%s'\n", optarg);
			progParam.outputFilePath = optarg;
			break;
		case 'a':
			printf ("option a with value '%s'\n", optarg);
			progParam.a = atoi(optarg);
			break;
		case 'c':
			printf ("option c with value '%s'\n", optarg);
			progParam.c = atoi(optarg);
			break;
		case 'm':
			printf ("option m with value '%s'\n", optarg);
			progParam.m = atoi(optarg);
			break;
		case 'x':
			printf ("option x with value '%s'\n", optarg);
			progParam.seed = atoi(optarg);
			break;
		case '?':
			break;
		default:
			printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}
	if (optind < argc) {
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		printf ("\n");
	}
	//#1END

	int num_thread = sysconf(_SC_NPROCESSORS_ONLN); // Количествео процессоров +1?
	cout<<"processes online" << num_thread;
	int inputFile = open(progParam.inputFilePath, O_RDONLY); // open to read inputfile

	if (inputFile == -1) // Проверка на корректность отрытия файла
	{
		std::cerr << "error with file 123 ";
		exit(ERROR_FILE_R_OPEN);
	}

	int inputSize = lseek(inputFile, 0, SEEK_END); // Узнаём размер файла в байтах
	std::cout<<"input size = "<<inputSize<<std::endl; 

	if(inputSize == -1)
	{
		std::cout << "error with calculation size of file ";
		exit(ERROR_FILE);
	}

	char* key = new char[inputSize]; // Место для ПСП 
	char* outputText = new char[inputSize]; //Зашифрованный текст
	char* msg = new char[inputSize]; // Текст из inputFile

	if(lseek(inputFile, 0, SEEK_SET) == -1) // Возвращаемся в начало файла, чтобы прочитать его с начала
	{
		std::cout << "error with file ";
		exit(ERROR_FILE);
	}

	inputSize = read(inputFile, msg, inputSize); //Помещаем в  msg текст из inputFile 

	if(inputSize == -1) // Проверка успешности перемещения  в буффер
	{
		std::cout << "error with moving text into buffer msg ";
		exit(ERROR_FILE);
	}

	keygenParam keyParam; // Дублируем параметры для ЛКГ
	keyParam.sizeKey = inputSize;
	keyParam.a=progParam.a;
	keyParam.c=progParam.c;
	keyParam.m=progParam.m;
	keyParam.seed=progParam.seed;

	pthread_t keyGenThread;//создаём отдельный поток для ЛКГ
	pthread_t cryptThread[num_thread];// создаём массив потоков для шифрования 
	int status = 0;

	if(pthread_create(&keyGenThread, NULL, keyGenerate, &keyParam) != 0) //Создаем и проверяем успешность потока
	{
		std::cout << "error with pthread_create()";
		exit(ERROR_CREATE_THREAD);
	}
	if(pthread_join(keyGenThread, (void**)&key) != 0)// Запускаем поток и отлавливаем результат работы потока keyGenThread
	{
		std::cout << "error with pthread_join()";
		exit(ERROR_JOIN_THREAD);
	}

	pthread_barrier_t barrier; //Создаём барьер

	status = pthread_barrier_init(&barrier, NULL, num_thread+1); //Задаём параметры для работы барьера 

	if(status != 0)// Проверяем успешность инициализации работы барьера
	{
		std::cout << "error with pthread_barrier_init()";
		exit(ERROR_BARRIER);
	}


	std::vector<cryptParam*> cryptPar;
	// down and topindex нужны для того чтобы "Случайно разбить текст на фрагменты ",
	//Данная "случайность" зависит от количества ядер, и она же помогает нам 
	//производить обратную трансляцию. Чекай шифр Вермана.
	for(int i = 0; i < num_thread ; i++)//создание воркеров #7
	{
		cryptParam* cryptParametrs = new cryptParam; //контекст для воркеров #6

		cryptParametrs->key = key;
		cryptParametrs->size = inputSize;
		cryptParametrs->outputText = outputText;
		cryptParametrs->msg = msg;
		cryptParametrs->barrier = &barrier;

        size_t current_len = inputSize / num_thread;
        
        cryptParametrs->downIndex = i * current_len;
        cryptParametrs->topIndex = i * current_len + current_len;

        if (i == num_thread - 1)
        {
            cryptParametrs->topIndex = inputSize;
        }

		cryptPar.push_back(cryptParametrs);
		pthread_create(&cryptThread[i], NULL, crypt, cryptParametrs);

	}
	status = pthread_barrier_wait(&barrier);// #8

	int output;
	if ((output=open(progParam.outputFilePath, O_WRONLY))==-1) {
		printf ("Cannot open file.\n");
		exit(ERROR_FILE_W_OPEN);
	}
	if(write(output, outputText, inputSize) !=inputSize)
		printf("Write Error");
	close(output);

	//работа с памятью, очистка
	pthread_barrier_destroy(&barrier);

	delete[] key;
	delete[] outputText;
	delete[] msg;

	for(auto & tempCryptParam : cryptPar){
		delete tempCryptParam;
	}

	return SUCCESS;
}
