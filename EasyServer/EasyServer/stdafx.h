// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <winsock2.h>
// 윈도우 소켓 사용을 위한 헤더

#include <windows.h>

#include <process.h>
// 스레드 생성을 위해 불러온 헤더

#include <assert.h>
// 디버깅을 위한 assert용 헤더
//
// assert(조건문)을 선언하면
// 디버깅 모드에서 조건 이외의 값이 들어오면 즉시 프로그램이 멈춤
// http://tylee80.blog.me/30164137998 참조

#include <limits.h>
// 숫자 표현 크기 제한 정의 헤더
// http://ko.wikipedia.org/wiki/Limits.h
// http://blog.naver.com/rasede/150082570531 참조

#include <vector>

#include <algorithm>
// 알고리즘 라이브러리 헤더
// STL 사용 할 때 함께 사용
// http://en.cppreference.com/w/cpp/algorithm 참조

#include <atomic>
// std::atmoic 을 사용하기 위한 헤더
// 연산의 원자성(atomic)을 보장하여 멀티 스레드 작업 안정성을 높임
//
// http://yh120.tistory.com/10
// http://yesarang.tistory.com/376
// http://sweeper.egloos.com/viewer/3059861
// http://neive.tistory.com/400 참조

#include <typeinfo>
// 현재 객체의 클래스 타입을 알기 위한 typeid() 를 사용하기 위한 헤더

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.
