# 요구사항 정의서

## 과제 목표

`memo.py` 의 내용을 채워서 메모장이 동작하게 구현하고 이를 AWS 에 Application load balancer 를 이용해 서비스를 구성하는 것이 과제의 목표입니다.


## 제출물

본인 github repo 에 `memo_server` 라는 서브폴더를 만들어서 다음 파일들을 제출하세요.
* 완성된 `memo.py` 를 포함해서 수정 혹은 추가한 파일들
* 실행 방법 및 코드 설명을 담고 있는 `readme.md` 파일
* `AWS` 상에 동작하고 있는 배포된 실행 환경


## 평가 항목

* AWS 의 application load balancer 의 DNS 주소를 통해서 서비스가 잘 동작하는가?
- 네이버 OAuth 가 제대로 구현되었는가?
- `GET /memo` 가 제대로 구현되었는가?
-  `POST /memo` 가 제대로 구현되었는가?
-  지시한 대로 AWS 에 서비스가 구성되었는가?



## 목차

### Part1: memo.py 구현하기

### Part2: DB 사용

### Part3: AWS 에 서비스 구성



# Part1: memo.py 구현하기

## 1) `def home()`

`userId` 쿠키가 설정되어 있는 경우 DB 에서 해당 유저의 이름을 읽어와서 `index.html` template 에 반영하는 동작이 누락되어 있습니다.

## 2) `def onOAuthAuthorizationCodeRedirected()`

현재 `def onOAuthAuthorizationCodeRedirected()` 의 내용은 비어있습니다. 해당 함수에 코멘트로 표시된 대로 단계 1 ~ 4 까지를 채워 넣어야 합니다.

## 3) `def getMemos()`

메모 목록을 DB 로부터 읽어오는 부분이 빠져있습니다. 현재 로그인한 유저의 메모들을 읽어올 수 있어야 합니다.

## 4) `def post_new_memo()`

새 메모를 DB 에 저장하는 부분이 빠져있습니다. 현재 로그인한 유저의 메모로 저장되어야 합니다.



# Part2: DB 사용

DB 는 본인이 원하는 DB 중 어떤 것이라도 쓸 수 있습니다. (예: MySQL, MariaDB, Redis, MongoDB) DB 를 하나만 써도 되고 필요하다면 여러 DB 를 사용해도 무방합니다.


## 1) 실습 서버에서 DB 컨테이너 띄우기

수업 시간에 docker 에 대해서 배운대로 DB 를 띄우면 됩니다. 다른 학생과 포트가 겹치지 않도록 `-p` 옵션을 이용해 50000 + 실습번호 형태로 사용하기 바랍니다.

## 2) AWS 에 서비스를 구성할 때 DB 띄우기

DB 용 Ubuntu 가상 서버를 하나 띄우고, 그 가상 서버에서 docker 를 이용해 동일한 DB 를 띄웁니다.
가상 서버를 만든 후 Docker 의 설치는 다음 명령을 이용합니다.
```
$ sudo apt-get install docker.io
```
그리고 docker 명령을 할 때는 `sudo` 를 앞에 붙여서 실행하세요. (예: `$ sudo docker container ps`)
원래 docker 는 관리자 권한이 있어야 되는데, 실습 서버에서는 교수가 여러분 아이디가 sudo 없이 docker 를 수행할 수 있게 설정했었습니다. 그때문에 AWS 에서 가상 서버를 만든 경우에는 `sudo` 를 앞에 붙여 줘야 합니다.

구현한 메모 서비스는 DB 서버에 접근할 때 DB 서버의 private IP 를 이용합니다. public IP 를 이용하거나 Elastic IP 를 부여해서 접근하는 경우는 감점 처리 되니 주의하세요.



# Part3: AWS 에 서비스 구성

## 1) 서비스 서버 구성

* 교수가 공유한 `mjubackend` AMI 를 이용해서 가상 서버를 만듭니다.
* 이 때 서비스 서버는 ssh 와 http 만 열려 있도록 security group 을 설정합니다.
* `mjubackend` AMI 이미지는 TCP 80 번 포트에 nginx 를 동작시키고 있습니다.
* nginx 설정에서는 `/memo` 라는 경로가 들어오면 `127.0.0.1:30001` 를 통해서 uwsgi 를 호출하게 되어있습니다.
* 127.0.0.1:30001 에는 uwsgi 가 동작하고 있습니다. 이 때 uwsgi 의 설정 파일은 홈 디렉터리 아래 있는 `uwsgi.ini` 파일입니다. (`$ netstat -an | grep 30001` 로 확인해보세요)
* 서비스로 돌고 있는 uwsgi 는 홈디렉터리 아래 있는 `/memo` 경로에 대해서 `memo.py` 를 호출합니다.
* 따라서 여러분이 작성한 `memo.py`, `references/`, `templates/` 을 복사해 오면 별다른 문제 없이 `http://서비스서버publicIP/memo` 접근 가능 여부를 확인해볼 수 있습니다.
* **(중요)**: 담당 교수가 서비스 서버에 로그인해볼 수 있도록 다음의 SSH public key 를 `authorized_keys` 파일에 추가합니다. ```ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIHyI345QnkwdhuOcV/AUTYbxKZ8u1ayqjzduSCsQ6jAd dkmoon@dkmoon-desktop```

## 2) DB 서버 만들기

* DB 용 Ubuntu 가상 서버를 만들고 docker 를 이용해 여러분이 사용한 DB 를 띄웁니다.
* 서비스 서버는 DB 서버에 private IP 를 통해서 접속하도록 설정합니다.
* **(중요)**: 담당 교수가 서비스 서버에 로그인해볼 수 있도록 다음의 SSH public key 를 `authorized_keys` 파일에 추가합니다. ```ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIHyI345QnkwdhuOcV/AUTYbxKZ8u1ayqjzduSCsQ6jAd dkmoon@dkmoon-desktop```
  
## 3) 서비스 서버 이미지 만들기

* 여러분이 작성한 `memo.py`, `references/`, `templates/` 을 복사한 뒤 가상 서버를 커스터마이징 했으면, 이 가상 서버를 이용해 AMI 이미지를 만들도록 합니다.
* 여기서 만들어진 이미지를 이용해 launch template 을 작성할 겁니다.

## 4) Launch template 작성

* 앞에서 만든 AMI 를 통해서 launch template 을 작성합니다.
* 사양은 `t2.micro` 로 하고, ssh 와 http 만 열려 있도록 security group 을 설정합니다.

## 5) Target group (서버팜) 생성

* 대상 그룹 (target group) 으로 server farm 을 만듭니다.

## 6) Application 로드 밸런서 생성

* 앞서 생성한 target group 을 위해서 Application 로드 밸런서를 만듧니다.

## 7) Auto scaling 설정

* Auto scaling group 을 만들어서 로드 밸런서가 최대 1대부터 최대 2대까지 서버를 자동 생성하게 설정합니다.

## 주의할 점

* `memo.py` 를 복사해올 때 redirect URI 나 DB 주소가 하드코딩 되어있을 경우 AWS 에 서비스를 구성할 때는 이들을 모두 수정해야 합니다.



# 구현 자가 진단 리스트

## **Part1: memo.py 구현 점검**
1. **home()**
   - [v] `userId` 쿠키 확인 여부 구현 (이미 구현됨)
   - [ ] DB에서 `userId`에 해당하는 유저 이름 조회 기능 구현
   - [ ] 조회된 유저 이름을 `index.html` template에 반영

2. **onOAuthAuthorizationCodeRedirected()**
   - [v] OAuth 2.0 인증 코드 수신 및 처리
   - [v] 네이버 API를 통한 엑세스 토큰 요청 구현
   - [ ] 엑세스 토큰으로 사용자 정보 조회 기능 구현
   - [ ] DB에 유저 정보 저장 
   - [v] 세션/쿠키 설정

3. **getMemos()**
   - [ ] 로그인 유저의 메모 조회 기능 구현
   - [ ] DB에서 메모 목록 조회 및 JSON 응답 생성

4. **post_new_memo()**
   - [ ] 로그인 유저의 새 메모 저장 기능 구현
   - [ ] 메모 내용을 DB에 저장 및 결과 응답

## **Part2: DB 사용 점검**
1. **Docker DB 컨테이너 실행**
   - [ ] 실습 서버에서 Docker 컨테이너 실행 및 포트 충돌 방지
   - [ ] DB와 `memo.py` 간 연결 확인

2. **AWS DB 서버 구성**
   - [ ] AWS DB 전용 Ubuntu 가상 서버 생성
   - [ ] Docker를 통한 동일한 DB 구성
   - [ ] 서비스 서버와 DB 간 private IP 연결 설정

## **Part3: AWS 서비스 구성 점검**
1. **서비스 서버 설정**
   - [ ] `mjubackend` AMI를 기반으로 가상 서버 생성
   - [ ] nginx 및 uwsgi가 정상 작동 확인
   - [ ] `memo.py`, `references/`, `templates/` 복사 및 배포 확인
   - [ ] SSH public key 추가 확인

2. **DB 서버 설정**
   - [ ] DB 서버에서 Docker를 이용해 DB 실행
   - [ ] 서비스 서버에서 private IP를 통한 DB 접근 확인
   - [ ] SSH public key 추가 확인

3. **서비스 서버 이미지 생성**
   - [ ] 커스터마이징된 서비스 서버 AMI 이미지 생성 확인

4. **Launch Template 생성**
   - [ ] t2.micro 사양으로 launch template 작성
   - [ ] Security Group: SSH와 HTTP만 허용

5. **Target Group 생성**
   - [ ] Target Group에 서비스 서버 등록 확인

6. **Application Load Balancer 구성**
   - [ ] Target Group과 연동된 Application Load Balancer 생성
   - [ ] DNS를 통한 서비스 정상 동작 확인

7. **Auto Scaling 설정**
   - [ ] 최소 1대, 최대 2대 서버 생성으로 Auto Scaling Group 설정
   - [ ] 서버 상태에 따라 Auto Scaling 동작 확인

## **최종 점검**
- [ ] `GET /memo` API 호출 정상 동작
- [ ] `POST /memo` API 호출 정상 동작
- [ ] 네이버 OAuth 기능 정상 작동
- [ ] Application Load Balancer의 DNS 주소에서 서비스 접속 가능
- [ ] 제출물(GitHub repo) 구성 및 README 작성 완료
