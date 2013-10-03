-- Simple leader-election protocol based on a static store (etcd).
--
-- Each round, a node checks whether there's a current leader.  If there is
-- none, it "nominates" itself by trying to set the value, but this nomination
-- is only TENTATIVE.  If a node checks and sees its own TENTATIVE nomination,
-- it tries to make that PERMANENT.  A TENTATIVE nomination can also be
-- overridden by *another* node if that second node is "more senior" to the
-- first.  We use the node number as a substitute for seniority, but it could
-- be any value that means we think one node would make a better leader.
--
-- The TENTATIVE state isn't necessary to make the protocol work.  You can
-- change the code to go straight to PERMANENT and everything will still work.
-- However, if you enable the "Brag" rule and run with -pr you can see that
-- having the state makes it more likely that the "right" node will become
-- leader whenever possible.
--
-- We also allow some nodes to fail during the election.  We can remain
-- oblivious to the failure only up to a maximum of MAX_FAIL_TIME rounds, but
-- after that long handling is mandatory.
--
-- Assumptions/shortcuts:
--	messages aren't delayed by more than one clock tick (e.g. second)
--	failure detection within MAX_FAIL_TIME is guaranteed

Const
	NUM_NODES:		3;
	-- Normally NONE/TENTATIVE/PERMANENT would be an Enum, but Murphi won't
	-- allow arithmetic that way and incrementing is convenient.
	NONE:			0;
	TENTATIVE:		1;
	PERMANENT:		2;
	NUM_FAILURES:		3;
	MAX_FAIL_TIME:		5;
	-- Our stability check won't work properly if we allow failures too
	-- late in the game, so just allow enough for them to be sequential.
	TICK_LIMIT:		(MAX_FAIL_TIME * NUM_NODES) + 10;

Type
	NodeCount:		0..NUM_NODES;
	NodeNum:		1..NUM_NODES;
	LeaderType:		NONE..PERMANENT;
	LeaderValue: Record
		lv_type:	LeaderType;
		lv_num:		NodeCount;
	End;
	LeaderReq: Record
		lr_sender:	NodeNum;
		lr_old:		LeaderValue;
		lr_new:		LeaderValue;
		lr_update:	Boolean;
	End;
	LeaderRsp: Record
		lr_value:	LeaderValue;
		lr_changed:	Boolean;
	End;
	MsgState:		Enum { IDLE, TICKED, WAITING, FULL };
	Node: Record
		state:		LeaderType;
		msg_state:	MsgState;
		msg:		LeaderRsp;
		failed:		Boolean;
		skipped:	0..MAX_FAIL_TIME;
	End;
	DataStore: Record
		value:		LeaderValue;
		msgs:		Array [ NodeNum ] Of LeaderReq;
		filled:		Array [ NodeNum ] Of Boolean;
	End;

Var
	Nodes:			Array [ NodeNum ] Of Node;
	Etcd:			DataStore;
	Ticks:			0..TICK_LIMIT;
	FailsLeft:		0..NUM_FAILURES;
	Bragged:		Boolean;


Function ValuesEqual (val1: LeaderValue; val2: LeaderValue): Boolean;
Begin
	If val1.lv_type != val2.lv_type Then
		Return False;
	End;
	If val1.lv_num != val2.lv_num Then
		Return False;
	End;
	Return True;
End;


-- Try to become leader if there's no leader at all, or if there's only a
-- nomination and we're more senior.
Function ShouldTry (msg: LeaderRsp; n: NodeNum): Boolean;
Begin
	Switch msg.lr_value.lv_type
	Case NONE:
		Return True;
	Case TENTATIVE:
		Return (n >= msg.lr_value.lv_num);
	End;
	Return False;
End;


Function CountLeaders (): NodeCount;
Var
	res: NodeCount;
Begin
	res := 0;
	For n: NodeNum Do
		If Nodes[n].state = PERMANENT Then
			res := res + 1;
		End;
	End;
	Return res;
End;


-- In addition to making sure there aren't *too many* leaders, we want to make
-- sure that we do have one and the store reflects it.
Function CorrectFinalState (): Boolean;
Begin
	Return (Etcd.value.lv_type = PERMANENT) & (CountLeaders() = 1);
End;


Procedure DeliverReq (req: LeaderReq);
Begin
	Etcd.msgs[req.lr_sender] := req;
	Etcd.filled[req.lr_sender] := True;
End;


Procedure DeliverRsp (who: NodeNum; msg: LeaderRsp);
	If Nodes[who].failed Then
		Return;
	End;
	If Nodes[who].msg_state = WAITING Then
		Nodes[who].msg := msg;
		Nodes[who].msg_state := FULL;
	End;
End;


Procedure HandleFailure (n: NodeNum);
Begin
	-- Simulate TTL expiration.
	If Etcd.value.lv_num = n Then
		Etcd.value.lv_type := NONE;
		Etcd.value.lv_num := 0;
	End;
	-- Throw away any pending messages.
	Undefine Etcd.msgs[n];
	Etcd.filled[n] := False;
	-- Allow the node to come back.
	Nodes[n].failed := False;
	Undefine Nodes[n].skipped;
End;

Rule "Clock Tick"
	Forall n: NodeNum Do Nodes[n].msg_state = IDLE End
==>
	If Ticks < TICK_LIMIT Then
		For n: NodeNum Do
			If !Nodes[n].failed Then
				Nodes[n].msg_state := TICKED;
			Elsif Nodes[n].skipped = MAX_FAIL_TIME Then
				HandleFailure(n);
			Else
				Nodes[n].skipped := Nodes[n].skipped + 1;
			End;
		End;
		Ticks := Ticks + 1;
		-- Recognize the failure if we've run out of time.
		If Ticks > (MAX_FAIL_TIME * NUM_NODES) Then
			FailsLeft := 0;
		End;
	End;
End;

Ruleset n: NodeNum Do

	Rule "Send Request"
		Nodes[n].msg_state = TICKED
	==>
	Var
		r: LeaderReq;
	Begin
		-- Send a GET (lr_update=false)
		r.lr_sender := n;
		r.lr_update := False;
		DeliverReq(r);
		Nodes[n].msg_state := WAITING;
	End;

	-- This could be a GET or a conditional PUT.  Modify the in-store value
	-- iff we're asked (lr_update=true) and the old value matches.  If we
	-- did execute a PUT, set lr_changed=true.  Always return the *new*
	-- value (same as old for a GET or failed PUT).
	Rule "Handle Request"
		Etcd.filled[n]
	==>
	Var
		r: LeaderRsp;
	Begin
		Alias m: Etcd.msgs[n] Do
			If m.lr_update & ValuesEqual(m.lr_old,Etcd.value) Then
				Etcd.value := m.lr_new;
				r.lr_changed := True;
			Else
				r.lr_changed := False;
			End;
			r.lr_value.lv_type := Etcd.value.lv_type;
			r.lr_value.lv_num := Etcd.value.lv_num;
			DeliverRsp(m.lr_sender,r);
			Undefine m;
		End;
		Etcd.filled[n] := False;
	End;

	Rule "Handle Response"
		Nodes[n].msg_state = FULL
	==>
	Var
		r: LeaderReq;
	Begin
		Alias m: Nodes[n].msg Do
			Nodes[n].msg_state := IDLE;
			If m.lr_changed Then
				-- Our nomination/consolidation was accepted.
				Nodes[n].state := m.lr_value.lv_type;
			Elsif ShouldTry(m,n) Then
				-- No permanent leader yet, and we think we're
				-- the best candidate.
				r.lr_sender := n;
				r.lr_old.lv_type := m.lr_value.lv_type;
				r.lr_old.lv_num := m.lr_value.lv_num;
				r.lr_new.lv_type := Nodes[n].state + 1;
				r.lr_new.lv_num := n;
				r.lr_update := True;
				DeliverReq(r);
				Nodes[n].msg_state := WAITING;
			End;
			Undefine m;
		End;
	End;

	Rule "Fail"
		(FailsLeft > 0) & ! Nodes[n].failed
	==>
		Nodes[n].state := NONE;
		Nodes[n].msg_state := IDLE;
		Nodes[n].failed := TRUE;
		Nodes[n].skipped := 0;
		FailsLeft := FailsLeft - 1;
	End;

	-- Recognize a failure before it becomes mandatory to do so.
	Rule "Recognize Failure"
		Nodes[n].failed
	==>
		HandleFailure(n);
	End;

	Rule "Brag"
		(Ticks = TICK_LIMIT) & (Nodes[n].state = PERMANENT) & !Bragged
	==>
		Bragged := True;
	End;
End;

Startstate Begin
	For n: NodeNum Do
		Nodes[n].state := NONE;
		Nodes[n].msg_state := IDLE;
		Nodes[n].failed := False;
		Etcd.filled[n] := False;
	End;
	Etcd.value.lv_type := NONE;
	Etcd.value.lv_num := 0;
	Ticks := 0;
	FailsLeft := NUM_FAILURES;
	Bragged := False;
End;

Invariant "Only One"
	CountLeaders() <= 1;

Invariant "Reach Stability"
	(Ticks < TICK_LIMIT) | CorrectFinalState();
