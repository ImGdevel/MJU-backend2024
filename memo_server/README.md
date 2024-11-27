# README

## 코드 설명

home) Redis에서 랜덤키에 해당하는 userid를 받아온뒤 DB에서 유저 정보를 가져옵니다.
auth) 네이버 로그인 API를 통해 로그인합니다.
get) Redis에서 랜덤키에 해당하는 userid를 받아온 뒤 DB에 메모를 가져옵니다.
post) edis에서 랜덤키에 해당하는 userid를 받아온 뒤 DB에 메모를 저장합니다.

간단한 Error 메시지와 Database 연결 부분은 GPT의 도움을 받았습니다. 

<br>

## Database 설명
 
유저, 메모장 등 영구적 데이터의 경우 MySQL Database
랜덤키와 같은 일시적 세션은 Redis에서 관리합니다.

MySQL은 3306 포트를, Redis는 6379 포트를 사용합니다.

SQLAlchemy ORM을 사용하기 떄문에
MySQL을 사용하기 위해서 추가적으로 flask_sqlalchemy 설치가 필요 합니다.

```
$ pip install flask_sqlalchemy
```

<br>

## AWS 설명

인스턴스

Web-Server-Dev는 개발용 서비스 서버

DB-Server는 데이터베이스(MySQL, Redis) 서버입니다.

Name이 붙지 않은 두개의 서버가 AutoScaleing으로 자동생성된 서버입니다. 

Web-Server-Dev와 자동 생성된 서버와 차이가 없긴 하지만 
Web-Server-Dev는 8000번 포트로 열어두었기 떄문에 웹서지스 이용시 8000번 포트로 접근해야합니다.
자동 생성된 서버는 로드 밸런서 DNS로 접근가능합니다.

<br>

## 사이트 접속

메모장 서비스가 돌아가는 url 링크 입니다. (로드 밸런서 링크)

```
http://memo-server-load-balancer-823271627.ap-northeast-2.elb.amazonaws.com/memo/
```

<br><br><br><br>

## 구현 자가 진단 리스트

### **memo.py 구현 점검**
1. **home()**
   - [V] DB에서 `userId`에 해당하는 유저 이름 조회 기능 구현
   - [V] 조회된 유저 이름을 `index.html` template에 반영

2. **onOAuthAuthorizationCodeRedirected()**
   - [v] OAuth 2.0 인증 코드 수신 및 처리
   - [v] 네이버 API를 통한 엑세스 토큰 요청 구현
   - [V] 엑세스 토큰으로 사용자 정보 조회 기능 구현
   - [V] DB에 유저 정보 저장 
   - [v] 세션/쿠키 설정

3. **getMemos()**
   - [V] 로그인 유저의 메모 조회 기능 구현
   - [V] DB에서 메모 목록 조회 및 JSON 응답 생성

4. **post_new_memo()**
   - [V] 로그인 유저의 새 메모 저장 기능 구현
   - [V] 메모 내용을 DB에 저장 및 결과 응답

## **AWS 서비스 구성 점검**
1. **서비스 서버 설정**
   - [V] `mjubackend` AMI를 기반으로 가상 서버 생성
   - [V] nginx 및 uwsgi가 정상 작동 확인
   - [V] `memo.py`, `static/`, `templates/` 복사 및 배포 확인
   - [V] SSH public key 추가 확인

2. **DB 서버 설정**
   - [V] DB 서버에서 Docker를 이용해 DB 실행
   - [V] 서비스 서버에서 private IP를 통한 DB 접근 확인
   - [V] SSH public key 추가 확인

3. **서비스 서버 이미지 생성**
   - [V] 커스터마이징된 서비스 서버 AMI 이미지 생성 확인

4. **Launch Template 생성**
   - [V] t2.micro 사양으로 launch template 작성
   - [V] Security Group: SSH와 HTTP만 허용

5. **Target Group 생성**
   - [V] Target Group에 서비스 서버 등록 확인

6. **Application Load Balancer 구성**
   - [V] Target Group과 연동된 Application Load Balancer 생성
   - [V] DNS를 통한 서비스 정상 동작 확인

7. **Auto Scaling 설정**
   - [V] 최소 1대, 최대 2대 서버 생성으로 Auto Scaling Group 설정
   - [V] 서버 상태에 따라 Auto Scaling 동작 확인

### **최종 점검**
- [V] `GET /memo` API 호출 정상 동작
- [V] `POST /memo` API 호출 정상 동작
- [V] 네이버 OAuth 기능 정상 작동
- [V] Application Load Balancer의 DNS 주소에서 서비스 접속 가능
- [V] 제출물(GitHub repo) 구성 및 README 작성 완료

### **추가 유의사항**
- [V] authoried key에 교수 public key 추가
- [V] 네이버 로그인 API에 mjubackend 추가 
