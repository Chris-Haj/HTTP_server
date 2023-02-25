import random
import webbrowser
import os

chrome = '/usr/bin/google-chrome %s'

def browser():
    port = random.randint(1000,3000)
    url = 'http://localhost:' + str(port)
    os.system('gcc server.c threadpool.c -o removeSpaces')
    print(f'{port}')
    webbrowser.get(chrome).open(url)
    os.system(f'./removeSpaces {port} 20 20')


if __name__ == '__main__':
    browser()



