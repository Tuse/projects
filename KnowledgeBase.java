import java.util.ArrayList;

/*
 * Store all the facts we know from the input file. Use to check whether other predicates are true.
 */
public class KnowledgeBase {
	private Predicate query;
	private ArrayList<Rule> rules;
	
	public KnowledgeBase(Predicate query, ArrayList<Rule> rules) {
		this.query = query; 
		this.rules = rules;
	}
	
	public Predicate getQuery() {
		return this.query;
	}
	
	//get rules from the KB that correlate with the current goal we are checking the truth of 
	public ArrayList<Rule> fetchRules(Predicate goal) {
		ArrayList<Rule> rules = new ArrayList<Rule>();
		
		for(Rule currRule : this.rules) {
			String rhsName = currRule.getRHS().getName();
			
			if(rhsName.equals(goal.getName())) {
				if(nonVarArgsMatch(goal, currRule.getRHS()))
				rules.add(currRule);
			}
		}
		
		return rules;
	}
	
	//Do the constants in two predicates match?
	private boolean nonVarArgsMatch(Predicate goal, Predicate KBPred) {
		//When here, we know the predicates have the same number of arguments
		boolean match = true;
		
		for(int i = 0; i < KBPred.getNumArgs(); i++) {
			String currKBPredArg = KBPred.getArgs().get(i);
			String currGoalArg = goal.getArgs().get(i);
			
			//check if the args, from both the KB and the goal predicates, are constants
			boolean KBPredArgConstant = Character.isUpperCase((currKBPredArg.charAt(0)));
			boolean goalArgConstant = Character.isUpperCase((currGoalArg.charAt(0)));
			
			//If both predicate args are constant, check if equal
			if(KBPredArgConstant && goalArgConstant) {
				if(!currKBPredArg.equals(currGoalArg)) {
					match = false;
				}
			}
		}
		
		return match;
	}
}
